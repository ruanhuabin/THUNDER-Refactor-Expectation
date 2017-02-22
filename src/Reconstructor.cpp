/*******************************************************************************
 * Author: Hongkun Yu, Mingxu Hu
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#include "Reconstructor.h"

Reconstructor::Reconstructor()
{
    defaultInit();
}

Reconstructor::Reconstructor(const int mode,
                             const int size,
                             const int pf,
                             const Symmetry* sym,
                             const double a,
                             const double alpha)
{
    defaultInit();

    init(mode, size, pf, sym, a, alpha);
}

Reconstructor::~Reconstructor()
{
    _fft.fwDestroyPlanMT();
    _fft.bwDestroyPlanMT();
}

void Reconstructor::init(const int mode,
                         const int size,
                         const int pf,
                         const Symmetry* sym,
                         const double a,
                         const double alpha)
{
    _mode = mode;
    _size = size;
    _pf = pf;
    _sym = sym;

    /***
    _FSC = vec::Constant(1, 1);

    _sig = vec::Zero(1);

    _tau = vec::Constant(1, 1);
    ***/

    _a = a;
    _alpha = alpha;

    // initialise the interpolation kernel

    _kernelFT.init(boost::bind(MKB_FT_R2,
                               boost::placeholders::_1,
                               _pf * _a,
                               _alpha),
                   0,
                   gsl_pow_2(_pf * _a),
                   1e5);

    _kernelRL.init(boost::bind(MKB_RL_R2,
                               boost::placeholders::_1,
                               _pf * _a,
                               _alpha),
                   0,
                   1,
                   1e5);

    _maxRadius = (_size / 2 - a);

    if (_mode == MODE_2D)
    {
        _F2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
        _W2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
        _C2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);
        _T2D.alloc(PAD_SIZE, PAD_SIZE, FT_SPACE);

        _fft.fwCreatePlanMT(PAD_SIZE, PAD_SIZE);
        _fft.bwCreatePlanMT(PAD_SIZE, PAD_SIZE);
    }
    else if (_mode == MODE_3D)
    {
        _F3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
        _W3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
        _C3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);
        _T3D.alloc(PAD_SIZE, PAD_SIZE, PAD_SIZE, FT_SPACE);

        _fft.fwCreatePlanMT(PAD_SIZE, PAD_SIZE, PAD_SIZE);
        _fft.bwCreatePlanMT(PAD_SIZE, PAD_SIZE, PAD_SIZE);
    }
    else 
        REPORT_ERROR("INEXISTENT MODE");

    reset();
}

void Reconstructor::reset()
{
    /***
    _rot.clear();
    _w.clear();
    _ctf.clear();
    ***/

    _iCol = NULL;
    _iRow = NULL;
    _iPxl = NULL;
    _iSig = NULL;

    _calMode = POST_CAL_MODE;

    _MAP = true;

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for
        SET_0_FT(_F2D);

        #pragma omp parallel for
        SET_1_FT(_W2D);

        #pragma omp parallel for
        SET_0_FT(_C2D);

        #pragma omp parallel for
        SET_0_FT(_T2D);
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for
        SET_0_FT(_F3D);

        #pragma omp parallel for
        SET_1_FT(_W3D);

        #pragma omp parallel for
        SET_0_FT(_C3D);

        #pragma omp parallel for
        SET_0_FT(_T3D);
    }
    else
        REPORT_ERROR("INEXISTENT MODE");
}

int Reconstructor::mode() const
{
    return _mode;
}

void Reconstructor::setMode(const int mode)
{
    _mode = mode;
}

bool Reconstructor::MAP() const
{
    return _MAP;
}

void Reconstructor::setMAP(const bool MAP)
{
    _MAP = MAP;
}

void Reconstructor::setSymmetry(const Symmetry* sym)
{
    _sym = sym;
}

void Reconstructor::setFSC(const vec& FSC)
{
    _FSC = FSC;
}

void Reconstructor::setTau(const vec& tau)
{
    _tau = tau;
}

void Reconstructor::setSig(const vec& sig)
{
    _sig = sig;
}

int Reconstructor::maxRadius() const
{
    return _maxRadius;
}

void Reconstructor::setMaxRadius(const int maxRadius)
{
    _maxRadius = maxRadius;
}

void Reconstructor::preCal(int& nPxl,
                           const int* iCol,
                           const int* iRow,
                           const int* iPxl,
                           const int* iSig) const
{
    nPxl = _nPxl;

    iCol = _iCol;
    iRow = _iRow;
    iPxl = _iPxl;
    iSig = _iSig;
}

void Reconstructor::setPreCal(const int nPxl,
                              const int* iCol,
                              const int* iRow,
                              const int* iPxl,
                              const int* iSig)
{
    _calMode = PRE_CAL_MODE;

    _nPxl = nPxl;

    _iCol = iCol;
    _iRow = iRow;
    _iPxl = iPxl;
    _iSig = iSig;
}

void Reconstructor::insert(const Image& src,
                           const Image& ctf,
                           const mat22& rot,
                           const vec2& t,
                           const double w)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_2D REPORT_ERROR("WRONG MODE");

    if (_calMode != POST_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");

    if ((src.nColRL() != _size) ||
        (src.nRowRL() != _size) ||
        (ctf.nColRL() != _size) ||
        (ctf.nRowRL() != _size))
        REPORT_ERROR("INCORRECT SIZE OF INSERTING IMAGE");
#endif

    Image transSrc(_size, _size, FT_SPACE);
    translateMT(transSrc, src, _maxRadius, -t(0), -t(1));

    #pragma omp parallel for schedule(dynamic)
    IMAGE_FOR_EACH_PIXEL_FT(transSrc)
    {
        if (QUAD(i, j) < gsl_pow_2(_maxRadius))
        {
            vec2 newCor((double)(i * _pf), (double)(j * _pf));
            vec2 oldCor = rot * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F2D.addFT(transSrc.getFTHalf(i, j)
                     * REAL(ctf.getFTHalf(i, j))
                     * w, 
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a, 
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F2D.addFT(transSrc.getFTHalf(i, j)
                     * REAL(ctf.getFTHalf(i, j))
                     * w, 
                       oldCor(0), 
                       oldCor(1));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _T2D.addFT(gsl_pow_2(REAL(ctf.getFTHalf(i, j)))
                     * w, 
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a,
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
             _T2D.addFT(gsl_pow_2(REAL(ctf.getFTHalf(i, j)))
                      * w, 
                        oldCor(0), 
                        oldCor(1));
#endif

#endif
        }
    }
}

void Reconstructor::insert(const Image& src,
                           const Image& ctf,
                           const mat33& rot,
                           const vec2& t,
                           const double w)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_3D REPORT_ERROR("WRONG MODE");

    if (_calMode != POST_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");

    if ((src.nColRL() != _size) ||
        (src.nRowRL() != _size) ||
        (ctf.nColRL() != _size) ||
        (ctf.nRowRL() != _size))
        REPORT_ERROR("INCORRECT SIZE OF INSERTING IMAGE");
#endif

    Image transSrc(_size, _size, FT_SPACE);
    translateMT(transSrc, src, _maxRadius, -t(0), -t(1));

    vector<mat33> sr;
#ifdef RECONSTRUCTOR_SYMMETRIZE_DURING_INSERT
    symmetryRotation(sr, rot, _sym);
#else
    sr.push_back(rot);
#endif

    for (int k = 0; k < int(sr.size()); k++)
    {
        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(transSrc)
        {
            if (QUAD(i, j) < gsl_pow_2(_maxRadius))
            {
                vec3 newCor((double)(i * _pf), (double)(j * _pf), 0);
                vec3 oldCor = sr[k] * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
                _F3D.addFT(transSrc.getFTHalf(i, j)
                         * REAL(ctf.getFTHalf(i, j))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2), 
                           _pf * _a, 
                           _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
                _F3D.addFT(transSrc.getFTHalf(i, j)
                         * REAL(ctf.getFTHalf(i, j))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
                _T3D.addFT(gsl_pow_2(REAL(ctf.getFTHalf(i, j)))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2),
                           _pf * _a,
                           _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
                _T3D.addFT(gsl_pow_2(REAL(ctf.getFTHalf(i, j)))
                         * w, 
                           oldCor(0), 
                           oldCor(1), 
                           oldCor(2));
#endif

#endif
            }
        }
    }
}

void Reconstructor::insertP(const Image& src,
                            const Image& ctf,
                            const mat22& rot,
                            const vec2& t,
                            const double w)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_2D REPORT_ERROR("WRONG MODE");

    if (_calMode != PRE_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");
#endif

    Image transSrc(_size, _size, FT_SPACE);
    translateMT(transSrc, src, -t(0), -t(1), _iCol, _iRow, _iPxl, _nPxl);

        #pragma omp parallel for
        for (int i = 0; i < _nPxl; i++)
        {
            vec2 newCor((double)(_iCol[i] * _pf), (double)(_iRow[i] * _pf));
            vec2 oldCor = rot * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F2D.addFT(transSrc[_iPxl[i]]
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a, 
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F2D.addFT(transSrc[_iPxl[i]]
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _T2D.addFT(gsl_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       _pf * _a,
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _T2D.addFT(gsl_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * w,
                       oldCor(0), 
                       oldCor(1));
#endif

#endif
        }
}

void Reconstructor::insertP(const Image& src,
                            const Image& ctf,
                            const mat33& rot,
                            const vec2& t,
                            const double w)
{
#ifdef RECONSTRUCTOR_ASSERT_CHECK
    IF_MASTER
        REPORT_ERROR("INSERTING IMAGES INTO RECONSTRUCTOR IN MASTER");

    NT_MODE_3D REPORT_ERROR("WRONG MODE");

    if (_calMode != PRE_CAL_MODE)
        REPORT_ERROR("WRONG PRE(POST) CALCULATION MODE IN RECONSTRUCTOR");
#endif

    Image transSrc(_size, _size, FT_SPACE);
    translateMT(transSrc, src, -t(0), -t(1), _iCol, _iRow, _iPxl, _nPxl);

    vector<mat33> sr;
#ifdef RECONSTRUCTOR_SYMMETRIZE_DURING_INSERT
    symmetryRotation(sr, rot, _sym);
#else
    sr.push_back(rot);
#endif

    for (int k = 0; k < int(sr.size()); k++)
    {
        #pragma omp parallel for
        for (int i = 0; i < _nPxl; i++)
        {
            vec3 newCor((double)(_iCol[i] * _pf), (double)(_iRow[i] * _pf), 0);
            vec3 oldCor = sr[k] * newCor;

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F3D.addFT(transSrc[_iPxl[i]]
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2), 
                       _pf * _a, 
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F3D.addFT(transSrc[_iPxl[i]]
                     * REAL(ctf.iGetFT(_iPxl[i]))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2));
#endif

#ifdef RECONSTRUCTOR_ADD_T_DURING_INSERT

#ifdef RECONSTRUCTOR_MKB_KERNEL
            _T3D.addFT(gsl_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2),
                       _pf * _a,
                       _kernelFT);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _T3D.addFT(gsl_pow_2(REAL(ctf.iGetFT(_iPxl[i])))
                     * w,
                       oldCor(0), 
                       oldCor(1), 
                       oldCor(2));
#endif

#endif
        }
    }
}

void Reconstructor::reconstruct(Image& dst)
{
    Volume tmp;

    reconstruct(tmp);

    dst.alloc(PAD_SIZE, PAD_SIZE, RL_SPACE);

    SLC_EXTRACT_RL(dst, tmp, 0);
}

void Reconstructor::reconstruct(Volume& dst)
{
    IF_MASTER return;

    IF_MODE_2D
    {
        ALOG(INFO, "LOGGER_RECO") << "Reconstructing Under 2D Mode";
        BLOG(INFO, "LOGGER_RECO") << "Reconstructing Under 2D Mode";
    }

    IF_MODE_3D
    {
        ALOG(INFO, "LOGGER_RECO") << "Reconstructing Under 3D Mode";
        BLOG(INFO, "LOGGER_RECO") << "Reconstructing Under 3D Mode";
    }

    ALOG(INFO, "LOGGER_RECO") << "Allreducing T";
    BLOG(INFO, "LOGGER_RECO") << "Allreducing T";

    allReduceT();

    // only in 3D mode, symmetry should be considered
    IF_MODE_3D
    {
#ifdef RECONSTRUCTOR_SYMMETRIZE_DURING_RECONSTRUCT
        ALOG(INFO, "LOGGER_RECO") << "Symmetrizing T";
        BLOG(INFO, "LOGGER_RECO") << "Symmetrizing T";

        symmetrizeT();
#endif
    }

    // only in 3D mode, the MAP method is appropriate
    if (_MAP && (_mode == MODE_3D))
    {
        // Obviously, wiener_filter with FSC can be wrong when dealing with
        // preferrable orienation problem
#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC

#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC_FREQ_AVG
        vec avg = vec::Zero(_maxRadius * _pf + 1);
        shellAverage(avg,
                     _T3D,
                     gsl_real,
                    _maxRadius * _pf - 1);
        // the last two elements have low fidelity
        avg(_maxRadius * _pf - 1) = avg(_maxRadius * _pf - 2);
        avg(_maxRadius * _pf) = avg(_maxRadius * _pf - 2);

        ALOG(INFO, "LOGGER_SYS") << "End of Avg = "
                                 << avg(avg.size() - 5) << ", "
                                 << avg(avg.size() - 4) << ", "
                                 << avg(avg.size() - 3) << ", "
                                 << avg(avg.size() - 2) << ", "
                                 << avg(avg.size() - 1);
        BLOG(INFO, "LOGGER_SYS") << "End of Avg = "
                                 << avg(avg.size() - 4) << ", "
                                 << avg(avg.size() - 3) << ", "
                                 << avg(avg.size() - 2) << ", "
                                 << avg(avg.size() - 1);
#endif

        ALOG(INFO, "LOGGER_SYS") << "End of FSC = " << _FSC(_FSC.size() - 1);
        BLOG(INFO, "LOGGER_SYS") << "End of FSC = " << _FSC(_FSC.size() - 1);

        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_T3D)
            if ((QUAD_3(i, j, k) >= gsl_pow_2(WIENER_FACTOR_MIN_R * _pf)) &&
                (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf)))
            {
                int u = AROUND(NORM_3(i, j, k));

                double FSC = (u >= _FSC.size())
                           ? _FSC(_FSC.size() - 1)
                           : _FSC(u);

                FSC = GSL_MAX_DBL(1e-3, GSL_MIN_DBL(1 - 1e-3, FSC));

                FSC = sqrt(2 * FSC / (1 + FSC));

#ifdef RECONSTRUCTOR_WIENER_FILTER_FSC_FREQ_AVG
                _T3D.setFT(_T3D.getFT(i, j, k)
                         + COMPLEX((1 - FSC) / FSC * avg(u), 0),
                           i,
                           j,
                           k);
#else
                _T3D.setFT(_T3D.getFT(i, j, k) / FSC, i, j, k);
#endif
            }
#endif

#ifdef RECONSTRUCTOR_WIENER_FILTER_CONST
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_T3D)
            if ((QUAD_3(i, j, k) >= gsl_pow_2(WIENER_FACTOR_MIN_R * _pf)) &&
                (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf)))
                _T3D.setFT(_T3D.getFT(i, j, k) + COMPLEX(1, 0), i, j, k);
#endif
    }

    ALOG(INFO, "LOGGER_RECO") << "Initialising W";
    BLOG(INFO, "LOGGER_RECO") << "Initialising W";

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for
        IMAGE_FOR_EACH_PIXEL_FT(_W2D)
            if (QUAD(i, j) < gsl_pow_2(_maxRadius * _pf))
                _W2D.setFTHalf(COMPLEX(1, 0), i, j);
            else
                _W2D.setFTHalf(COMPLEX(0, 0), i, j);
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for
        VOLUME_FOR_EACH_PIXEL_FT(_W3D)
            if (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf))
                _W3D.setFTHalf(COMPLEX(1, 0), i, j, k);
            else
                _W3D.setFTHalf(COMPLEX(0, 0), i, j, k);
    }
    else
        REPORT_ERROR("INEXISTENT MODE");

    double diffC = DBL_MAX;
    double diffCPrev = DBL_MAX;

    for (int m = 0; ; m++)
    {
        ALOG(INFO, "LOGGER_RECO") << "Balancing Weights Round " << m;
        BLOG(INFO, "LOGGER_RECO") << "Balancing Weights Round " << m;

        ALOG(INFO, "LOGGER_RECO") << "Determining C";
        BLOG(INFO, "LOGGER_RECO") << "Determining C";
        
        if (_mode == MODE_2D)
        {
            #pragma omp parallel for
            FOR_EACH_PIXEL_FT(_C2D)
                _C2D[i] = _T2D[i] * _W2D[i];
        }
        else if (_mode == MODE_3D)
        {
            #pragma omp parallel for
            FOR_EACH_PIXEL_FT(_C3D)
                _C3D[i] = _T3D[i] * _W3D[i];
        }
        else
            REPORT_ERROR("INEXISTENT MODE");

        convoluteC();

        ALOG(INFO, "LOGGER_RECO") << "Calculating Distance to Total Balanced";
        BLOG(INFO, "LOGGER_RECO") << "Calculating Distance to Total Balanced";
        
        diffCPrev = diffC;

        diffC = checkC();

        ALOG(INFO, "LOGGER_RECO") << "Distance to Total Balanced: " << diffC;
        BLOG(INFO, "LOGGER_RECO") << "Distance to Total Balanced: " << diffC;

        if ((m >= N_ITER_BALANCE) &&
            (diffC > diffCPrev * DIFF_C_DECREASE_THRES)) break;
        else
        {
            if (_mode == MODE_2D)
            {
                #pragma omp parallel for schedule(dynamic)
                IMAGE_FOR_EACH_PIXEL_FT(_W2D)
                    if (QUAD(i, j) < gsl_pow_2(_maxRadius * _pf))
                        _W2D.setFTHalf(_W2D.getFTHalf(i, j)
                                     / GSL_MAX_DBL(ABS(_C2D.getFTHalf(i, j)),
                                                   1e-6),
                                       i,
                                       j);
            }
            else if (_mode == MODE_3D)
            {
                #pragma omp parallel for schedule(dynamic)
                VOLUME_FOR_EACH_PIXEL_FT(_W3D)
                    if (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf))
                        _W3D.setFTHalf(_W3D.getFTHalf(i, j, k)
                                     / GSL_MAX_DBL(ABS(_C3D.getFTHalf(i, j, k)),
                                                   1e-6),
                                       i,
                                       j,
                                       k);
            }
            else
                REPORT_ERROR("INEXISTENT MODE");
        }
    }

    ALOG(INFO, "LOGGER_RECO") << "Allreducing F";
    BLOG(INFO, "LOGGER_RECO") << "Allreducing F";

    allReduceF();

    // only in 3D mode, symmetry should be considered
    IF_MODE_3D
    {
#ifdef RECONSTRUCTOR_SYMMETRIZE_DURING_RECONSTRUCT
        ALOG(INFO, "LOGGER_RECO") << "Symmetrizing F";
        BLOG(INFO, "LOGGER_RECO") << "Symmetrizing F";

        symmetrizeF();
#endif
    }

    if (_mode == MODE_2D)
        _fft.bwExecutePlan(_F2D);
    else if (_mode == MODE_3D)
        _fft.bwExecutePlan(_F3D);
    else
        REPORT_ERROR("INEXISTENT MODE");

#ifdef RECONSTRUCTOR_CORRECT_CONVOLUTION_KERNEL

    ALOG(INFO, "LOGGER_RECO") << "Correcting Convolution Kernel";
    BLOG(INFO, "LOGGER_RECO") << "Correcting Convolution Kernel";

#ifdef RECONSTRUCTOR_MKB_KERNEL
    double nf = MKB_RL(0, _a * _pf, _alpha);
#endif

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_RL(_F2D)
        {
#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F2D.setRL(_F2D.getRL(i, j)
                     / MKB_RL(NORM(i, j) / PAD_SIZE,
                              _a * _pf,
                              _alpha)
                     * nf,
                       i,
                       j);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F2D.setRL(_F2D.getRL(i, j)
                     / TIK_RL(NORM(i, j) / PAD_SIZE),
                       i,
                       j);
#endif
        }
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_RL(_F3D)
        {
#ifdef RECONSTRUCTOR_MKB_KERNEL
            _F3D.setRL(_F3D.getRL(i, j, k)
                     / MKB_RL(NORM_3(i, j, k) / PAD_SIZE,
                              _a * _pf,
                              _alpha)
                     * nf,
                       i,
                       j,
                       k);
#endif

#ifdef RECONSTRUCTOR_TRILINEAR_KERNEL
            _F3D.setRL(_F3D.getRL(i, j, k)
                     / TIK_RL(NORM_3(i, j, k) / PAD_SIZE),
                       i,
                       j,
                       k);
#endif
        }
    }
    else
        REPORT_ERROR("INEXISTENT MODE");

    ALOG(INFO, "LOGGER_RECO") << "Convolution Kernel Corrected";
    BLOG(INFO, "LOGGER_RECO") << "Convolution Kernel Corrected";

#endif

    if (_mode == MODE_2D)
    {
        dst.clear();
        dst.alloc(_size * _pf, _size * _pf, 1, RL_SPACE);
        COPY_RL(dst, _F2D);

        _fft.fwExecutePlanMT(_F2D);
        _F2D.clearRL();
    }
    else if (_mode == MODE_3D)
    {
        dst = _F3D.copyVolume();

        _fft.fwExecutePlanMT(_F3D);
        _F3D.clearRL();
    }
    else
        REPORT_ERROR("INEXISTENT MODE");

#ifdef RECONSTRUCTOR_REMOVE_NEG
    ALOG(INFO, "LOGGER_RECO") << "Removing Negative Values";
    BLOG(INFO, "LOGGER_RECO") << "Removing Negative Values";

    #pragma omp parallel for
    REMOVE_NEG(dst);
#endif
}

void Reconstructor::allReduceF()
{
    if (_mode == MODE_2D)
    {
        #pragma omp parallel for
        MUL_FT(_F2D, _W2D);
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for
        MUL_FT(_F3D, _W3D);
    }
    else
        REPORT_ERROR("INEXISTENT MODE");

    ALOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere A";
    BLOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere B";

    MPI_Barrier(_hemi);

    if (_mode == MODE_2D)
        MPI_Allreduce_Large(&_F2D[0],
                            _F2D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else if (_mode == MODE_3D)
        MPI_Allreduce_Large(&_F3D[0],
                            _F3D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else
        REPORT_ERROR("INEXISTENT MODE");

    MPI_Barrier(_hemi);
}

void Reconstructor::allReduceT()
{
    ALOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere A";
    BLOG(INFO, "LOGGER_RECO") << "Waiting for Synchronizing all Processes in Hemisphere B";

    MPI_Barrier(_hemi);

    if (_mode == MODE_2D)
        MPI_Allreduce_Large(&_T2D[0],
                            _T2D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else if (_mode == MODE_3D)
        MPI_Allreduce_Large(&_T3D[0],
                            _T3D.sizeFT(),
                            MPI_DOUBLE_COMPLEX,
                            MPI_SUM,
                            _hemi);
    else
        REPORT_ERROR("INEXISTENT MODE");

    MPI_Barrier(_hemi);
}

double Reconstructor::checkC() const
{
#ifdef RECONSTRUCTOR_CHECK_C_AVERAGE
    double diff = 0;

    int counter = 0;

    if (_mode == MODE_2D)
    {
        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(_C2D)
            if (QUAD(i, j) < gsl_pow_2(_maxRadius * _pf))
            {
                #pragma omp atomic
                diff += fabs(ABS(_C2D.getFT(i, j)) - 1);
                #pragma omp atomic
                counter += 1;
            }
    }
    else if (_mode == MODE_3D)
    {
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_C3D)
            if (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf))
            {
                #pragma omp atomic
                diff += fabs(ABS(_C3D.getFT(i, j, k)) - 1);
                #pragma omp atomic
                counter += 1;
            }
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");
        abort();
    }

    return diff / counter;
#endif

#ifdef RECONSTRUCTOR_CHECK_C_MAX
    if (_mode == MODE_2D)
    {
        vector<double> diff(_C2D.sizeFT(), 0);
        
        #pragma omp parallel for schedule(dynamic)
        IMAGE_FOR_EACH_PIXEL_FT(_C2D)
            if (QUAD(i, j) < gsl_pow_2(_maxRadius * _pf))
                diff[_C2D.iFTHalf(i, j)] = fabs(ABS(_C2D.getFT(i, j)) - 1);

        return *std::max_element(diff.begin(), diff.end());
    }
    else if (_mode == MODE_3D)
    {
        vector<double> diff(_C3D.sizeFT(), 0);

        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_FT(_C3D)
            if (QUAD_3(i, j, k) < gsl_pow_2(_maxRadius * _pf))
                diff[_C3D.iFTHalf(i, j, k)] = fabs(ABS(_C3D.getFT(i, j, k)) - 1);

        return *std::max_element(diff.begin(), diff.end());
    }
    else
    {
        REPORT_ERROR("INEXISTENT MODE");
        abort();
    }
#endif
}

void Reconstructor::convoluteC()
{
    double nf = MKB_RL(0, _a * _pf, _alpha);

    if (_mode == MODE_2D)
    {
        _fft.bwExecutePlanMT(_C2D);

        #pragma omp parallel for
        IMAGE_FOR_EACH_PIXEL_RL(_C2D)
            _C2D.setRL(_C2D.getRL(i, j)
                     * _kernelRL(QUAD(i, j) / gsl_pow_2(PAD_SIZE))
                     / nf,
                       i,
                       j);

        _fft.fwExecutePlanMT(_C2D);

        _C2D.clearRL();
    }
    else if (_mode == MODE_3D)
    {
        _fft.bwExecutePlanMT(_C3D);

        #pragma omp parallel for
        VOLUME_FOR_EACH_PIXEL_RL(_C3D)
            _C3D.setRL(_C3D.getRL(i, j, k)
                     * _kernelRL(QUAD_3(i, j, k) / gsl_pow_2(PAD_SIZE))
                     / nf,
                       i,
                       j,
                       k);

        _fft.fwExecutePlanMT(_C3D);

        _C3D.clearRL();
    }
    else
        REPORT_ERROR("INEXISTENT MODE");
}

void Reconstructor::symmetrizeF()
{
    if (_sym != NULL)
        SYMMETRIZE_FT(_F3D, _F3D, *_sym, _maxRadius * _pf + 1, SINC_INTERP);
    else
        CLOG(WARNING, "LOGGER_SYS") << "Symmetry Information Not Assigned in Reconstructor";
}

void Reconstructor::symmetrizeT()
{
    if (_sym != NULL)
        SYMMETRIZE_FT(_T3D, _T3D, *_sym, _maxRadius * _pf + 1, SINC_INTERP);
    else
        CLOG(WARNING, "LOGGER_SYS") << "Symmetry Information Not Assigned in Reconstructor";
}
