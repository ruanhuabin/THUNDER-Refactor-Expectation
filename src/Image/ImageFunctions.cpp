/*******************************************************************************
 * Author: Mingxu Hu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#include "ImageFunctions.h"

void translate(Image& dst,
               const Image& src,
               const double nTransCol,
               const double nTransRow)
{
    double rCol = nTransCol / src.nColRL();
    double rRow = nTransRow / src.nRowRL();

    IMAGE_FOR_EACH_PIXEL_FT(src)
    {
        double phase = 2 * M_PI * (i * rCol + j * rRow);
        dst.setFT(src.getFT(i, j) * COMPLEX_POLAR(-phase), i, j);
    }
}

/***
void meshReverse(Image& img)
{
    IMAGE_FOR_EACH_PIXEL_FT(img)
        if ((i + j) % 2 == 1)
            img.setFT(-img.getFT(i, j), i, j);
}
***/

/***
void meshReverse(Volume& vol)
{
    VOLUME_FOR_EACH_PIXEL_FT(vol)
        if ((i + j + k) % 2 == 1)
            vol.setFT(-vol.getFT(i, j, k), i, j, k);
}
***/

void meshReverse(Image& img)
{
    for (int j = 0; j < img.nRowFT(); j++)
        for (int i = 0; i < img.nColFT(); i++)
            if ((i + j) % 2 == 1)
                img[j * img.nColFT() + i] *= -1;
}

void meshReverse(Volume& vol)
{
    for (int k = 0; k < vol.nSlcFT(); k++)
        for (int j = 0; j < vol.nRowFT(); j++)
            for (int i = 0; i < vol.nColFT(); i++)
                if ((i + j + k) % 2 == 1)
                    vol[k * vol.nColFT() * vol.nRowFT()
                      + j * vol.nColFT()
                      + i] *= -1;
}

void bgMeanStddev(double& mean,
                  double& stddev,
                  const Image& src,
                  const double r)
{
    vector<double> bg;
    IMAGE_FOR_EACH_PIXEL_RL(src)
        if (NORM(i, j) > r)
            bg.push_back(src.getRL(i, j));

    vec bv(bg);
    mean = arma::mean(bv);
    stddev = arma::stddev(bv);
}

void removeDust(Image& img,
                const double wDust,
                const double bDust,
                const double mean,
                const double stddev)
{
    IMAGE_FOR_EACH_PIXEL_RL(img)
        if ((img.getRL(i, j) > mean + wDust * stddev) ||
            (img.getRL(i, j) < mean - bDust * stddev))
            img.setRL(mean + gsl_ran_gaussian(RANDR, stddev), i, j);
}

void normalise(Image& img,
               const double wDust,
               const double bDust,
               const double r)
{
    double mean;
    double stddev;

    bgMeanStddev(mean, stddev, img, r);

    removeDust(img, wDust, bDust, mean, stddev);

    bgMeanStddev(mean, stddev, img, r);

    gsl_vector vec;
    vec.size = img.sizeRL();
    vec.data = &img(0);
    
    gsl_vector_add_constant(&vec, -mean);
    gsl_vector_scale(&vec, 1.0 / stddev);
}

void extract(Image& dst,
             const Image& src,
             const int xOff,
             const int yOff)
{
    IMAGE_FOR_EACH_PIXEL_RL(dst)
        dst.setRL(src.getRL(i + xOff, j + yOff), i, j);
}

void slice(Image& dst,
           const Volume& src,
           const int iSlc)
{
    IMAGE_FOR_EACH_PIXEL_RL(dst)
        dst.setRL(src.getRL(i, j, iSlc), i, j);
}
