/*******************************************************************************
 * Author: Mingxu Hu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#include "Mask.h"

double background(const Image& img,
                  const double r,
                  const double ew)
{
    double weightSum = 0;
    double sum = 0;

    IMAGE_FOR_EACH_PIXEL_RL(img)
    {
        double u = NORM(i, j);

        if (u > r + ew)
        {
            weightSum += 1;
            sum += img.getRL(i, j);
        }
        else if (u >= r)
        {
            double w = 0.5 - 0.5 * cos((u - r) / ew * M_PI);
            weightSum += w;
            sum += img.getRL(i, j) * w;
        }
    }

    return sum / weightSum;
}

double background(const Image& img,
                  const Image& alpha)
{
    double weightSum = 0;
    double sum = 0;

    IMAGE_FOR_EACH_PIXEL_RL(img)
    {
        double w = 1 - alpha.getRL(i, j);
        weightSum += w;
        sum += img.getRL(i, j) * w;
    }

    return sum / weightSum;
}

double background(const Volume& vol,
                  const double r,
                  const double ew)
{
    double weightSum = 0;
    double sum = 0;
    
    #pragma omp parallel for
    VOLUME_FOR_EACH_PIXEL_RL(vol)
    {
        double u = NORM_3(i, j, k);

        if (u > r + ew)
        {
            #pragma omp critical
            weightSum += 1;

            #pragma omp critical
            sum += vol.getRL(i, j, k);
        }
        else if (u >= r)
        {
            double w = 0.5 - 0.5 * cos((u - r) / ew * M_PI);

            #pragma omp critical
            weightSum += w;

            #pragma omp critical
            sum += vol.getRL(i, j, k) * w;
        }
    }

    return sum / weightSum;
}

double background(const Volume& vol,
                  const Volume& alpha)
{
    double weightSum = 0;
    double sum = 0;

    #pragma omp parallel for
    VOLUME_FOR_EACH_PIXEL_RL(vol)
    {
        double w = 1 - alpha.getRL(i, j, k);

        #pragma omp critical
        weightSum += w;

        #pragma omp critical
        sum += vol.getRL(i, j, k) * w;
    }

    return sum / weightSum;
}

void softMask(Image& dst,
              const Image& src,
              const double r,
              const double ew)
{
    double bg = background(src, r, ew);

    IMAGE_FOR_EACH_PIXEL_RL(src)
    {
        double u = NORM(i, j);

        if (u > r + ew)
            dst.setRL(bg, i, j);
        else if (u >= r)
        {
            double w = 0.5 - 0.5 * cos((u - r) / ew * M_PI);
            dst.setRL(bg * (1 - w) + src.getRL(i, j) * w, i, j);
        }
        else
            dst.setRL(src.getRL(i, j), i, j);
    }
}

void softMask(Image& dst,
              const Image& src,
              const Image& alpha)
{
    double bg = background(src, alpha);

    IMAGE_FOR_EACH_PIXEL_RL(src)
    {
        double w = alpha.getRL(i, j);
        dst.setRL(bg * (1 - w) + w * src.getRL(i, j), i, j);
    }
}

void softMask(Image& dst,
              const Image& src,
              const double r,
              const double ew,
              const double bgMean,
              const double bgStd)
{
    auto engine = get_random_engine();

    IMAGE_FOR_EACH_PIXEL_RL(src)
    {
        double u = NORM(i, j);

        if (u < r)
            dst.setRL(src.getRL(i, j), i, j);
        else
        {
            double bg = bgMean + gsl_ran_gaussian(engine, bgStd);

            if (u > r + ew)
                dst.setRL(bg, i, j);
            else
            {
                double w = 0.5 - 0.5 * cos((u - r) / ew * M_PI);
                dst.setRL(bg * (1 - w) + src.getRL(i, j) * w, i, j);
            }
        }
    }
}

void softMask(Volume& dst,
              const Volume& src,
              const double r,
              const double ew)
{
    double bg = background(src, r, ew);

    #pragma omp parallel for
    VOLUME_FOR_EACH_PIXEL_RL(src)
    {
        double u = NORM_3(i, j, k);

        if (u > r + ew)
            dst.setRL(bg, i, j, k);
        else if (u >= r)
        {
            double w = 0.5 - 0.5 * cos((u - r) / ew * M_PI);
            dst.setRL(bg * (1 - w) + w * src.getRL(i, j, k), i, j, k);
        }
        else
            dst.setRL(src.getRL(i, j, k), i, j, k);
    }
}

void softMask(Volume& dst,
              const Volume& src,
              const Volume& alpha)
{
    double bg = background(src, alpha);

    softMask(dst, src, alpha, bg);
}

void softMask(Volume& dst,
              const Volume& src,
              const Volume& alpha,
              const double bg)
{
    #pragma omp parallel for
    VOLUME_FOR_EACH_PIXEL_RL(src)
    {
        double w = alpha.getRL(i, j, k);
        dst.setRL(bg * (1 - w) + w * src.getRL(i, j, k), i, j, k);
    }
}

void genMask(Volume& dst,
             const Volume& src,
             const double dt)
{
    double thres = gsl_stats_max(&src.iGetRL(0), 1, src.sizeRL()) * dt;

    #pragma omp parallel for
    VOLUME_FOR_EACH_PIXEL_RL(src)
        if (src.getRL(i, j, k) > thres)
            dst.setRL(1, i, j, k);
        else
            dst.setRL(0, i, j, k);
}

void genMask(Volume& dst,
             const Volume& src,
             const double dt,
             const double ext)
{
    genMask(dst, src, dt);

    Volume dstTmp = dst.copyVolume();

    int a = CEIL(abs(ext));

    if (ext > 0)
    {
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_RL(dst)
            if (dst.getRL(i, j, k) == 1)
                VOLUME_FOR_EACH_PIXEL_IN_GRID(a)
                    if (QUAD_3(x, y, z) < gsl_pow_2(a))
                        dstTmp.setRL(1, i + x, j + y, k + z);
    }
    else if (ext < 0)
    {
        #pragma omp parallel for schedule(dynamic)
        VOLUME_FOR_EACH_PIXEL_RL(dst)
            if (dst.getRL(i, j, k) == 0)
                VOLUME_FOR_EACH_PIXEL_IN_GRID(a)
                    if (QUAD_3(x, y, z) < gsl_pow_2(a))
                        dstTmp.setRL(0, i + x, j + y, k + z);
    }

    dst = move(dstTmp);
}

/***
void generateMask(Volume& dst,
                  const Volume& src,
                  const double densityThreshold,
                  const double extend,
                  const double ew)
{
    generateMask(dst, src, densityThreshold, extend);

    int ew = ceil(ew);

    auto distance = [&dst, ew](const double i,
                               const double j,
                               const double k)
    {
        double result = FLT_MAX;

        for (int z = -ew; z < ew; z++)
            for (int y = -ew; y < ew; y++)
                for (int x = - ew; x < ew; x++)
                    if (((x * x + y * y + z * z) < result * result) &&
                        (dst.get(i + x, j + y, k + z) == 1))
                        result = sqrt(x * x + y * y + z * z);

        return result;
    };

    VOLUME_FOR_EACH_PIXEL(dst)
    {
        double d = distance(i, j, k);
        if ((dst.get(i, j, k) != 1) && (d < ew))
            dst.set(0.5 + 0.5 * cos(d / ew * M_PI), i, j, k);
    }
}
***/
