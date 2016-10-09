/*******************************************************************************
 * Author: Mingxu Hu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <functional>

#include "Error.h"
#include "Typedef.h"

#include "Image.h"
#include "Volume.h"

using namespace std;

/**
 * This function returns the Nyquist resolution limit in Angstrom(-1).
 * @param pixelSize pixel size in Angstrom
 */
double nyquist(const double pixelSize);

/**
 * This function converts resolution from pixel to Angstrom(-1).
 * @param resP resolution in pixel
 * @param imageSize the size of image in pixel
 * @param pixelSize pixel size in Angstrom
 */
double resP2A(const double resP,
              const int imageSize,
              const double pixelSize);

/**
 * This function converts resolution from Angstrom(-1) to pixel.
 * @param resA resolution in Angstrom(-1)
 * @param imageSize the size of image in pixel
 * @param pixelSize pixel size in Angstrom
 */
double resA2P(const double resA,
              const int imageSize,
              const double pixelSize);

/**
 * This function converts a vector of resolution from pixel to Angstorm(-1).
 * @param res a vector of resolution
 * @param imgeSize the size of image in pixel
 * @param pixelSize pixel size in Angstrom
 */
void resP2A(vec& res,
            const int imageSize,
            const double pixelSize);

/**
 * This function converts a vector of resolution from Angstrom(-1) to pixel.
 * @param res a vector of resolution
 * @param imgeSize the size of image in pixel
 * @param pixelSize pixel size in Angstrom
 */
void resA2P(vec& res,
            const int imageSize,
            const double pixelSize);

/**
 * This function calculates the ring average at a certain resolution with a
 * given function.
 * @param resP resolution in pixel
 * @param img image in Fourier space
 * @param func With this function from complex to double, the ring average of 
 *             this image is calculated.
 */
double ringAverage(const int resP,
                   const Image& img,
                   const function<double(const Complex)> func); 

/**
 * This function calculates the ring average at a certain resolution with
 * a given function.
 * @param resP resolution in pixel
 * @param img image in Fourier space
 * @param func With this function from complex to complex, the ring average of 
 *             this image is calculated.
 */
Complex ringAverage(const int resP,
                    const Image& img,
                    const function<Complex(const Complex)> func);

/**
 * This function calculates the shell average at a certain resolution with a
 * given function.
 * @param resP resolution in pixel
 * @param vol volume in Fourier space
 * @param func With this function from complex to double, the shell average of 
 *             this volume is calculated.
 */
double shellAverage(const int resP,
                    const Volume& vol,
                    const function<double(const Complex)> func);

/**
 * This function calculates the power spectrum of a certain image within a
 * given spatial frequency.
 * @param src image in Fourier space
 * @param r upper boundary of spatial frequency in pixel
 */
void powerSpectrum(vec& dst,
                   const Image& src,
                   const int r);

/**
 * This function calculates the power spectrum of a certain volume within a
 * given spatial frequency.
 * @param src volume in Fourier space
 * @param r upper boundary of spatial frequency in pixel
 */
void powerSpectrum(vec& dst,
                   const Volume& src,
                   const int r);

/**
 * This functions calculates the FRC (Fourier Ring Coefficient) between two
 * images.
 * @param dst vector for storing the FRC size of which is the upper boundary of
 *        spatial frequency in pixel
 * @param A image in Fourier space
 * @param B image in Fourier space
 */
void FRC(vec& dst,
         const Image& A,
         const Image& B);

/** 
 * This functions calculates the FSC (Fourier Shell Coefficient) between two
 * volumes.
 * @param dst vector for storing the FSC size of which is the upper boundary of
 *        spatial frequency in pixel
 * @param A volume in Fourier space
 * @param B volume in Fourier space
 */
void FSC(vec& dst,
         const Volume& A,
         const Volume& B);

int resolutionP(const vec& dst,
                const double thres,
                const int pf = 1);

/***
void wilsonPlot(std::map<double, double>& dst,
                const int imageSize,
                const double pixelSize,
                const double upperResA,
                const double lowerResA,
                const Vector<double>& ps,
                const int step = 1);

void wilsonPlot(std::map<double, double>& dst,
                const Volume& volume,
                const double pixelSize,
                const double upperResA,
                const double lowerResA);
                ***/

#endif // SPECTRUM_H
