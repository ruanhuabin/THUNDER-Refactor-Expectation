/*******************************************************************************
 * Author: Mingxu Hu
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#include "CTF.h"

double CTF(const double f,
           const double voltage,
           const double defocus,
           const double Cs)
{
    double lambda = 12.2643247 / sqrt(voltage * (1 + voltage * 0.978466e-6));

    double K1 = M_PI * lambda;
    double K2 = M_PI / 2 * Cs * gsl_pow_3(lambda);

    double ki = -K1 * defocus * gsl_pow_2(f) + K2 * gsl_pow_4(f);

    return w1 * sin(ki) - w2 * cos(ki);
}

void CTF(Image& dst,
         const double pixelSize,
         const double voltage,
         const double defocusU,
         const double defocusV,
         const double theta,
         const double Cs)
{
    double lambda = 12.2643247 / sqrt(voltage * (1 + voltage * 0.978466e-6));

    //std::cout << "lambda = " << lambda << std::endl;

    double K1 = M_PI * lambda;
    double K2 = M_PI / 2 * Cs * gsl_pow_3(lambda);

    IMAGE_FOR_EACH_PIXEL_FT(dst)
    {
        double u = NORM(i / (pixelSize * dst.nColRL()),
                        j / (pixelSize * dst.nRowRL()));

        double angle = atan2(j, i) - theta;
        double defocus = -(defocusU + defocusV
                         + (defocusU - defocusV) * cos(2 * angle)) / 2;

        double ki = K1 * defocus * gsl_pow_2(u) + K2 * gsl_pow_4(u);

        dst.setFT(COMPLEX(w1 * sin(ki) - w2 * cos(ki), 0),
                  i,
                  j);
    }
}

void reduceCTF(Image& dst,
               const Image& src,
               const Image& ctf)
{
    FOR_EACH_PIXEL_FT(src)
    {
        double v = REAL(ctf.iGetFT(i));

        dst[i] = v * src.iGetFT(i) / (gsl_pow_2(v) + CTF_TAU);
    }
}

void reduceCTF(Image& dst,
               const Image& src,
               const Image& ctf,
               const int r)
{
    IMAGE_FOR_PIXEL_R_FT(r + 1)
        if (QUAD(i, j) < r * r)
        {
            double v = REAL(ctf.getFT(i, j));

            dst.setFT(v * src.getFT(i, j) / (gsl_pow_2(v) + CTF_TAU), i, j);
        }
}

void reduceCTF(Image& dst,
               const Image& src,
               const Image& ctf,
               const vec& sigma,
               const vec& tau,
               const int pf,
               const int snrR,
               const int r)
{
    IMAGE_FOR_PIXEL_R_FT(r + 1)
    {
        int u = AROUND(NORM(i, j));

        if (u < r)
        {
            double v = REAL(ctf.getFT(i, j));

            if (u < snrR)
                dst.setFT(v * src.getFT(i, j)
                        / (gsl_pow_2(v) + sigma(u) / tau(pf * u)),
                          i,
                          j);
            else
                dst.setFT(v * src.getFT(i, j) / (gsl_pow_2(v) + CTF_TAU), i, j);
        }
    }
}
