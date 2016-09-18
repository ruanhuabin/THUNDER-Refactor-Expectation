/*******************************************************************************
 * Author: Hongkun Yu, Mingxu Hu
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#include "Reconstructor.h"

Reconstructor::Reconstructor() {}

Reconstructor::Reconstructor(const int size,
                             const int pf,
                             const Symmetry* sym,
                             const double a,
                             const double alpha)
{
    init(size, pf, sym, a, alpha);
}

Reconstructor::~Reconstructor() {}

void Reconstructor::init(const int size,
                         const int pf,
                         const Symmetry* sym,
                         const double a,
                         const double alpha)
{
    _rot.clear();
    _w.clear();
    _ctf.clear();

    _size = size;
    _pf = pf;
    _sym = sym;
    _a = a;
    _alpha = alpha;

    // initialise the interpolation kernel
    _kernel.init(bind(MKB_FT_R2, _1, _pf * _a, _alpha),
                 0,
                 gsl_pow_2(_pf * _a),
                 1e5);

    _maxRadius = (_size / 2 - a);

    _F.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
    _W.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
    _C.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);

    SET_0_FT(_F);
    SET_1_FT(_W);
    SET_0_FT(_C);
}

void Reconstructor::setSymmetry(const Symmetry* sym)
{
    _sym = sym;
}

int Reconstructor::maxRadius() const
{
    return _maxRadius;
}

void Reconstructor::setMaxRadius(const int maxRadius)
{
    _maxRadius = maxRadius;
}

void Reconstructor::insert(const Image& src,
                           const Image& ctf,
                           const mat33& rot,
                           const vec2& t,
                           const double w)
{
    IF_MASTER
    {
        CLOG(WARNING, "LOGGER_SYS") << "Inserting Images into Reconstructor in MASTER";
        return;
    }

    if ((src.nColRL() != _size) ||
        (src.nRowRL() != _size) ||
        (ctf.nColRL() != _size) ||
        (ctf.nRowRL() != _size))
        CLOG(FATAL, "LOGGER_SYS") << "Incorrect Size of Inserting Image"
                                  << ": _size = " << _size
                                  << ", nCol = " << src.nColRL()
                                  << ", nRow = " << src.nRowRL();

    Image transSrc(_size, _size, FT_SPACE);
    translateMT(transSrc, src, -t(0), -t(1));

    vector<mat33> sr;
    symmetryRotation(sr, rot, _sym);

    for (int k = 0; k < int(sr.size()); k++)
    {
        _rot.push_back(sr[k]);
        _w.push_back(w);
        _ctf.push_back(&ctf);

        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(transSrc)
        {
            if (QUAD(i, j) < gsl_pow_2(_maxRadius))
            {
                vec3 newCor = {(double)i, (double)j, 0};
                vec3 oldCor = sr[k] * newCor * _pf;
        
                _F.addFT(transSrc.getFTHalf(i, j)
                       * REAL(ctf.getFTHalf(i, j))
                       * w, 
                         oldCor[0], 
                         oldCor[1], 
                         oldCor[2], 
                         _pf * _a, 
                         _kernel);
            }
        }
    }

    //CLOG(INFO, "LOGGER_SYS") << "Partial : _F[0] = " << REAL(_F[0]);
}

void Reconstructor::insert(const Image& src,
                           const Image& ctf,
                           const Coordinate5D coord,
                           const double w)
{
    mat33 rot;
    rotate3D(rot, coord.phi, coord.theta, coord.psi);

    vec2 t = {(double)coord.x, (double)coord.y};

    insert(src, ctf, rot, t, w);
}

void Reconstructor::reconstruct(Volume& dst)
{
    IF_MASTER return;

    for (int i = 0; i < N_ITER_BALANCE; i++)
    {
        ALOG(INFO, "LOGGER_RECO") << "Balancing Weights Round " << i;
        BLOG(INFO, "LOGGER_RECO") << "Balancing Weights Round " << i;

        allReduceW();
    }

    ALOG(INFO, "LOGGER_RECO") << "Reducing F";
    BLOG(INFO, "LOGGER_RECO") << "Reducing F";

    allReduceF();

    // make sure the scale correct
    // SCALE_FT(_F, _pf * sqrt(_pf * _size));

    dst = _F.copyVolume();

    //CLOG(INFO, "LOGGER_SYS") << "dst[0] = " << REAL(dst[0]);

    FFT fft;
    fft.bwMT(dst);

    /***
    CLOG(INFO, "LOGGER_SYS") << "sum(dst)" << dst.sizeRL()
                                            * gsl_stats_mean(&dst(0), 1, dst.sizeRL());
    ***/

    ALOG(INFO, "LOGGER_RECO") << "Correcting Convolution Kernel";
    BLOG(INFO, "LOGGER_RECO") << "Correcting Convolution Kernel";

    #pragma omp parallel for schedule(dynamic)
    VOLUME_FOR_EACH_PIXEL_RL(dst)
    {
        double r = NORM_3(i, j, k) / PAD_SIZE;

        if ((r > 0.25 / _pf * RECO_LOOSE_FACTOR) ||
            (dst.getRL(i, j, k) < 0))
            dst.setRL(0, i, j, k);

        /***
        if (r > 0.25 / _pf * RECO_LOOSE_FACTOR)
            dst.setRL(0, i, j, k);
        ***/

        /***
        if (r < 0.5 / _pf)
        {
            dst.setRL(dst.getRL(i, j, k)
                    / MKB_RL(r, _pf * _a, _alpha),
                      i,
                      j,
                      k);
        }
        else
            dst.setRL(0, i, j, k);
        ***/

        /***
        if ((r < 0.5 / _pf) &&
            (dst.getRL(i, j, k) > 0))
        {
            dst.setRL(dst.getRL(i, j, k)
                    / TIK_RL(r),
                      i,
                      j,
                      k);
        }
        else
            dst.setRL(0, i, j, k);
        ***/

        /***
        if ((r < 0.5 / _pf) &&
            (dst.getRL(i, j, k) > 0))
        {
            dst.setRL(dst.getRL(i, j, k)
                    / MKB_RL(r, _pf * _a, _alpha)
                    / TIK_RL(r),
                      i,
                      j,
                      k);
        }
        else
            dst.setRL(0, i, j, k);
        ***/
    }

    /***
    CLOG(INFO, "LOGGER_SYS") << "sum(dst)" << dst.sizeRL()
                                            * gsl_stats_mean(&dst(0), 1, dst.sizeRL());
    ***/
}

void Reconstructor::allReduceW()
{
    SET_0_FT(_C);

    ALOG(INFO, "LOGGER_RECO") << "Re-calculating C";
    BLOG(INFO, "LOGGER_RECO") << "Re-calculating C";

    #pragma omp parallel for
    for (int k = 0; k < int(_rot.size()); k++)
        for (int j = -_size / 2; j < _size / 2; j++)
            for (int i = 0; i <= _size / 2; i++)
            {
                if (QUAD(i, j) < gsl_pow_2(_maxRadius))
                {
                    vec3 newCor = {(double)i, (double)j, 0};
                    vec3 oldCor = _rot[k] * newCor * _pf;

                    _C.addFT(REAL(_W.getByInterpolationFT(oldCor[0],
                                                          oldCor[1],
                                                          oldCor[2],
                                                          LINEAR_INTERP))
                           * gsl_pow_2(REAL(_ctf[k]->getFTHalf(i, j)))
                           * _w[k],
                             oldCor[0],
                             oldCor[1],
                             oldCor[2],
                             _pf * _a,
                             _kernel);
                }
            }
    
    ALOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere A";
    BLOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere B";

    MPI_Barrier(_hemi);

    ALOG(INFO, "LOGGER_RECO") << "Allreducing C";
    BLOG(INFO, "LOGGER_RECO") << "Allreducing C";

    MPI_Allreduce_Large(&_C[0],
                        _C.sizeFT(),
                        MPI_DOUBLE_COMPLEX,
                        MPI_SUM,
                        _hemi);

    MPI_Barrier(_hemi);

    /***
    ALOG(INFO, "LOGGER_RECO") << "Symmetrizing C";
    BLOG(INFO, "LOGGER_RECO") << "Symmetrizing C";

    symmetrizeC();
    ***/

    ALOG(INFO, "LOGGER_RECO") << "Re-calculating W";
    BLOG(INFO, "LOGGER_RECO") << "Re-calculating W";

    /***
    if (_pf * _a <= sqrt(3) / 2)
    {
        CLOG(FATAL, "LOGGER_SYS") << "Parameter a of MKB Kernel is Too Small.";
        __builtin_unreachable();
    }
    ***/

    //double cThres = MKB_FT(sqrt(3) / 2, _pf * _a, _alpha);
    //double cThres = 0.2;
    //double cThres = 0.5;
    double cThres = 1;
    //double cThres = MKB_FT(1, _pf * _a, _alpha);
    /***
    double cThres = MKB_FT(_pf * _a * 0.2,
                           _pf * _a,
                           _alpha);
                           ***/

    #pragma omp parallel for schedule(dynamic)
    VOLUME_FOR_EACH_PIXEL_FT(_W)
        if (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf))
        {
            double c = REAL(_C.getFTHalf(i, j, k));
            if (c >= cThres)
            {
                /***
                _W.setFTHalf(2 * c * _W.getFTHalf(i, j, k) / (1 + gsl_pow_2(c)),
                             i,
                             j,
                             k);
                ***/
                _W.setFTHalf(_W.getFTHalf(i, j, k) / c,
                             i,
                             j,
                             k);
            }
            /***
            else if (c == 0)
                _W.setFTHalf(COMPLEX(0, 0), i, j, k);
            ***/
            /***
            else
            {
                //_W.setFTHalf(COMPLEX(0, 0), i, j, k);
                _W.setFTHalf(2 * c * _W.getFTHalf(i, j, k) / (1 + gsl_pow_2(c)),
                             i,
                             j,
                             k);
            }
            ***/
        }
        else
            _W.setFTHalf(COMPLEX(0, 0), i, j, k);

    //DEBUG
    //CLOG(INFO, "LOGGER_SYS") << "_W[0] = " << REAL(_W[0]);
}

void Reconstructor::allReduceF()
{
    MUL_FT(_F, _W);

    ALOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere A";
    BLOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere B";

    MPI_Barrier(_hemi);

    ALOG(INFO, "LOGGER_RECO") << "Allreducing F";
    BLOG(INFO, "LOGGER_RECO") << "Allreducing F";

    MPI_Allreduce_Large(&_F[0],
                        _F.sizeFT(),
                        MPI_DOUBLE_COMPLEX,
                        MPI_SUM,
                        _hemi);

    MPI_Barrier(_hemi);

    //CLOG(INFO, "LOGGER_SYS") << "Total : _F[0] = " << REAL(_F[0]);

    /***
    ALOG(INFO, "LOGGER_RECO") << "Symmetrizing F";
    BLOG(INFO, "LOGGER_RECO") << "Symmetrizing F";

    symmetrizeF();
    ***/
}

/***
void Reconstructor::symmetrizeF()
{
    if (_sym == NULL) return;

    Volume symF;
    SYMMETRIZE_FT(symF, _F, *_sym, _maxRadius * _pf);

    _F = std::move(symF);
}

void Reconstructor::symmetrizeC()
{
    if (_sym == NULL) return;

    Volume symC;
    SYMMETRIZE_FT(symC, _C, *_sym, _maxRadius * _pf);

    _C = std::move(symC);
}
***/
