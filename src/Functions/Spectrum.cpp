/*******************************************************************************
 * Author: Mingxu Hu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#include "Spectrum.h"

double nyquist(const double pixelSize)
{
    return 2 / pixelSize;
}

double resP2A(const double resP,
              const int imageSize,
              const double pixelSize)
{
    return resP / imageSize / pixelSize;
}

double resA2P(const double resA,
              const int imageSize,
              const double pixelSize)
{
    return resA * imageSize * pixelSize;
}

void resP2A(vec& res,
            const int imageSize,
            const double pixelSize)
{
    res /= imageSize * pixelSize;
}

void resA2P(vec& res,
            const int imageSize,
            const double pixelSize)
{
    res *= imageSize * pixelSize;
}

double ringAverage(const int resP,
                   const Image& img,
                   const function<double(const Complex)> func)
{
    double result = 0;
    int counter = 0;

    IMAGE_FOR_EACH_PIXEL_FT(img)
        if (AROUND(NORM(i, j)) == resP)
        {
            result += func(img.getFT(i, j));
            counter++;
        }

    return result / counter;
}

Complex ringAverage(const int resP,
                    const Image& img,
                    const function<Complex(const Complex)> func)
{
    Complex result = COMPLEX(0, 0);
    double counter = 0;

    IMAGE_FOR_EACH_PIXEL_FT(img)
        if (AROUND(NORM(i, j)) == resP)
        {
            result += func(img.getFT(i, j));
            counter += 1;
        }

    return result / counter;
}

double shellAverage(const int resP,
                    const Volume& vol,
                    const function<double(const Complex)> func)
{
    double result = 0;
    int counter = 0;

    VOLUME_FOR_EACH_PIXEL_FT(vol)
        if (AROUND(NORM_3(i, j, k)) == resP)
        {
            result += func(vol.getFT(i, j, k));
            counter++;
        }

    return result / counter;
}

void powerSpectrum(vec& dst,
                   const Image& src,
                   const int r)
{
    dst.setZero();

    uvec counter = uvec::Zero(dst.size());

    #pragma omp parallel for schedule(dynamic)
    IMAGE_FOR_EACH_PIXEL_FT(src)
    {
        if (QUAD(i, j) < gsl_pow_2(r))
        {
            int u = AROUND(NORM(i, j));

            if (u < r)
            {
                #pragma omp atomic
                dst(u) += ABS2(src.getFT(i, j));
                #pragma omp atomic
                counter(u) += 1;
            }
        }
    }

    #pragma omp parallel for
    for (int i = 0; i < r; i++)
        dst(i) /= counter(i);
}

void powerSpectrum(vec& dst,
                   const Volume& src,
                   const int r)
{
    dst.setZero();

    uvec counter = uvec::Zero(dst.size());

    #pragma omp parallel for schedule(dynamic)
    VOLUME_FOR_EACH_PIXEL_FT(src)
    {
        if (QUAD_3(i, j, k) < gsl_pow_2(r))
        {
            int u = AROUND(NORM_3(i, j, k));

            if (u < r)
            {
                #pragma omp atomic
                dst(u) += ABS2(src.getFT(i, j, k));
                #pragma omp atomic
                counter(u) += 1;
            }
        }
    }

    #pragma omp parallel for
    for (int i = 0; i < r; i++)
        dst(i) /= counter(i);
}

void FRC(vec& dst,
         const Image& A,
         const Image& B)
{
    vec vecS = vec::Zero(dst.size());
    vec vecA = vec::Zero(dst.size());
    vec vecB = vec::Zero(dst.size());

    IMAGE_FOR_EACH_PIXEL_FT(A)
    {
        int u = AROUND(NORM(i, j));
        if (u < dst.size())
        {
            vecS[u] += REAL(A.getFT(i, j) * CONJUGATE(B.getFT(i, j)));
            vecA[u] += ABS2(A.getFT(i, j));
            vecB[u] += ABS2(B.getFT(i, j));
        }
    }

    //dst = vecS.array() / sqrt(vecA.array() * vecB.array());

    for (int i = 0; i < dst.size(); i++)
    {
        double AB = sqrt(vecA(i) * vecB(i));
        if (AB == 0)
            dst(i) = 0;
        else
            dst(i) = vecS(i) / AB;
    }
}

void FSC(vec& dst,
         const Volume& A,
         const Volume& B)
{
    vec vecS = vec::Zero(dst.size());
    vec vecA = vec::Zero(dst.size());
    vec vecB = vec::Zero(dst.size());

    VOLUME_FOR_EACH_PIXEL_FT(A)
    {
        int u = AROUND(NORM_3(i, j, k));
        if (u < dst.size())
        {
            vecS[u] += REAL(A.getFT(i, j, k) * CONJUGATE(B.getFT(i, j, k)));
            vecA[u] += ABS2(A.getFT(i, j, k));
            vecB[u] += ABS2(B.getFT(i, j, k));
        }
    }

    for (int i = 0; i < dst.size(); i++)
    {
        double AB = sqrt(vecA(i) * vecB(i));
        if (AB == 0)
            dst(i) = 0;
        else
            dst(i) = vecS(i) / AB;
    }

    //dst = vecS.array() / sqrt(vecA.array() * vecB.array());
}

int resP(const vec& dst,
         const double thres,
         const int pf,
         const int rL)
{
    int result;

    for (result = rL; result < dst.size(); result++)
    {
        if (dst(result) < thres) break;
    }

    return (result - 1) / pf;
}

void randomPhase(Volume& dst,
                 const Volume& src,
                 const int r)
{
    auto engine = get_random_engine();

    VOLUME_FOR_EACH_PIXEL_FT(dst)
    {
        int u = AROUND(NORM_3(i, j, k));

        if (u > r)
        {
            double norm = ABS(src.getFT(i, j, k));
            double phase = gsl_ran_flat(engine, 0, 2 * M_PI);

            dst.setFT(gsl_complex_polar(norm, phase), i, j, k);
        }
        else
            dst.setFT(src.getFT(i, j, k), i, j, k);
    }
}

/***
void wilsonPlot(std::map<double, double>& dst,
                const int imageSize,
                const double pixelSize,
                const double upperResA,
                const double lowerResA,
                const Vector<double>& ps,
                const int step)
{
    for (int i = 0; i < ps.length(); i++)
    {
        double resA;
        int resP = i * step;

        // calculate the resolution in Angstrom(-1)
        resP2A(resA, resP, imageSize, pixelSize);

        if ((resA <= upperResA) &&
            (resA >= lowerResA) &&
            (resA < nyquist(pixelSize)))
            dst.insert(std::pair<double, double>(resA * resA, ps.get(i)));
    }
}

void wilsonPlot(std::map<double, double>& dst,
                const Volume& volume,
                const double pixelSize,
                const double upperResA,
                const double lowerResA)
{
    // calculate power spectrum
    Vector<double> ps(volume.nColumn() / 2);
    powerSpectrum(ps, volume);

    // get Wilson Plot
    wilsonPlot(dst, volume.nColumn(), pixelSize, upperResA, lowerResA, ps);
}
***/
