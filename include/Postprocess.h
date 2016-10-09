/*******************************************************************************
 * Author: Mingxu Hu, Bing Li
 * Dependecy:
 * Test:
 * Execution:
 * Description:
 * ****************************************************************************/

#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "Macro.h"
#include "Typedef.h"
#include "Logging.h"

#include "ImageFile.h"
#include "Image.h"
#include "Volume.h"

using namespace std;

class Postprocess
{
    private:

        Volume _mapA;

        Volume _mapB;

        Volume _mapI;

        Volume _mapAMask;

        Volume _mapBMask;

        Volume _mapARFMask;

        Volume _mapBRFMask;

        Volume _mask;

        vec _fsc;

        vec _fscMask;

        vec _fscRFMask;

    public:        

        Postprocess();

        Postprocess(const char mapAFilename[],
                    const char mapBFilename[]);

        void run();
};

#endif // PREPROCESS_H
