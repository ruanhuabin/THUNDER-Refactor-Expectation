/*******************************************************************************
 * Author: Mingxu Hu
 * Dependency:
 * Test:
 * Execution:
 * Description:
 *
 * Manual:
 * ****************************************************************************/

#include "ImageFile.h"

ImageFile::ImageFile() {}

ImageFile::ImageFile(const char* filename,
                     const char* option)
{
    _file = fopen(filename, option);
}

ImageFile::~ImageFile()
{
    clear();
}

void ImageFile::display() const
{
    printf("Num of Columns:               %6d\n", nCol());
    printf("Num of Rows:                  %6d\n", nRow());
    printf("Num of Slices(Frames):        %6d\n", nSlc());
    printf("Mode:                         %6d\n", mode());
    printf("Symmetry Data Size:           %6d\n", symmetryDataSize());

    printf("\n");
    printf("MRC Header Size:              %6d\n", sizeof(MRCHeader));
    printf("Num of Intervals along Column:%6d\n", _MRCHeader.mx);
    printf("Num of Intervals along Row:   %6d\n", _MRCHeader.my);
    printf("Num of Intervals along Slice: %6d\n", _MRCHeader.mz);
    printf("Cell Dimension (AA, Column):  %6.3f\n", _MRCHeader.cella[0]);
    printf("Cell Dimension (AA, Row):     %6.3f\n", _MRCHeader.cella[1]);
    printf("Cell Dimension (AA, Slice):   %6.3f\n", _MRCHeader.cella[2]);
    printf("Cell Angle (AA, Column):      %6.3f\n", _MRCHeader.cellb[0]);
    printf("Cell Angle (AA, Row):         %6.3f\n", _MRCHeader.cellb[1]);
    printf("Cell Angle (AA, Slice):       %6.3f\n", _MRCHeader.cellb[2]);
    printf("Axis along Column:            %6d\n", _MRCHeader.mapc);
    printf("Axis along Row:               %6d\n", _MRCHeader.mapr);
    printf("Axis along Slice:             %6d\n", _MRCHeader.maps);
    printf("Space Group:                  %6d\n", _MRCHeader.ispg);
    printf("Orgin (AA, Column):           %6.3f\n", _MRCHeader.origin[0]);
    printf("Orgin (AA, Row):              %6.3f\n", _MRCHeader.origin[1]);
    printf("Orgin (AA, Slice):            %6.3f\n", _MRCHeader.origin[2]);
}

int ImageFile::mode() const { return _metaData.mode; }

int ImageFile::nCol() const { return _metaData.nCol; }

int ImageFile::nRow() const { return _metaData.nRow; }

int ImageFile::nSlc() const { return _metaData.nSlc; }

int ImageFile::size() const
{
    return _metaData.nCol * _metaData.nRow * _metaData.nSlc;
}

int ImageFile::symmetryDataSize() const
{
    return _metaData.symmetryDataSize;
}

void ImageFile::setSize(const int nCol,
                        const int nRow,
                        const int nSlc)
{
    _metaData.nCol = nCol;
    _metaData.nRow = nRow;
    _metaData.nSlc = nSlc;
}

void ImageFile::readMetaData()
{
    readMetaDataMRC();
}

void ImageFile::readMetaData(const Image& src)
{
    setSize(src.nColRL(), src.nRowRL());
    _metaData.mode = 2;
}

void ImageFile::readMetaData(const Volume& src)
{
    setSize(src.nColRL(), src.nRowRL(), src.nSlcRL());
    _metaData.mode = 2;
}

void ImageFile::readImage(Image& dst,
                          const int iSlc,
                          const char* fileType)
{
    if (strcmp(fileType, "MRC") == 0)
    {
        if (iSlc < 0 || iSlc >= nSlc())
            REPORT_ERROR("Index of slice is out boundary.");
        readImageMRC(dst, iSlc);
    }
    else if (strcmp(fileType, "BMP") == 0)
    {
        if (iSlc != 0)
            REPORT_ERROR("When read in a BMP file, iSlc must be 0.");
        readImageBMP(dst);
    }
    else
        REPORT_ERROR("File type can not be recognized");
}

void ImageFile::readVolume(Volume& dst,
                           const char* fileType)
{
    readVolumeMRC(dst);
}

void ImageFile::writeImage(const char* dst,
                           const ImageBase& src)
{
    writeImageMRC(dst, src);
}

void ImageFile::clear()
{
    if (_file != NULL) 
    {
        fclose(_file);
        _file = NULL;
    }

    if (_symmetryData != NULL)
    {
        delete[] _symmetryData;
        _symmetryData = NULL;
    }
}

void ImageFile::readMetaDataMRC()
{
    CHECK_FILE_VALID;

    rewind(_file);

    if (fread(&_MRCHeader, 1, 1024, _file) != 1024)
        REPORT_ERROR("Fail to read in mrc header file.");

    _metaData.mode = _MRCHeader.mode;
        
    _metaData.nCol = _MRCHeader.nx;
    _metaData.nRow = _MRCHeader.ny;
    _metaData.nSlc = _MRCHeader.nz;

    _metaData.symmetryDataSize = _MRCHeader.nsymbt;
}

void ImageFile::readSymmetryData()
{
    if (symmetryDataSize() != 0)
    {
        if (fseek(_file, 1024, 0) != 0)
            REPORT_ERROR("Fail to read in an image.");

        _symmetryData = new char[symmetryDataSize()];

        if (fread(_symmetryData, 1, symmetryDataSize(), _file) == 0)
            REPORT_ERROR("Fail to read in an image.");
    }
}

void ImageFile::readImageMRC(Image& dst,
                             const int iSlc)
{
    readSymmetryData();

	dst.alloc(nCol(), nRow(), realSpace);

    size_t size = dst.sizeRL();

    SKIP_HEAD(size * iSlc);
	
    switch (mode())
    {
        case 0: READ_CAST(dst, char); break;
        case 1: READ_CAST(dst, short); break;
        case 2: READ_CAST(dst, float); break;
    }
}

void ImageFile::readImageBMP(Image& dst)
{
    CHECK_FILE_VALID(_file);

    BMP bmp;
    bmp.open(_file);
    bmp.readInHeader();

    dst.alloc(bmp.getWidth(), bmp.getHeight(), realSpace);
        
    rewind(_file);
    fseek(_file, bmp.getHeaderSize(), 0);

    if (bmp.getBitCount() == 8)
        READ_CAST(dst, unsigned char);
    else
        REPORT_ERROR("Unsupported BMP coding mode.");
}

void ImageFile::readVolumeMRC(Volume& dst)
{
    readSymmetryData();

	dst.alloc(nCol(), nRow(), nSlc(), realSpace);

    SKIP_HEAD(0);
	
    switch (mode())
    {
        case 0: READ_CAST(dst, char); break;
        case 1: READ_CAST(dst, short); break;
        case 2: READ_CAST(dst, float); break;
    }
}

void ImageFile::writeImageMRC(const char* dst,
                              const ImageBase& src)
{
    _file = fopen(dst, "w");

    MRCHeader header;
    header.mode = _metaData.mode;
    header.nx = _metaData.nCol;
    header.ny = _metaData.nRow;
    header.nz = _metaData.nSlc;
    header.mx = _metaData.nCol;
    header.my = _metaData.nRow;
    header.mz = _metaData.nSlc;
    header.cella[0] = 1;
    header.cella[1] = 1;
    header.cella[2] = 1;
    header.cellb[0] = 90;
    header.cellb[1] = 90;
    header.cellb[2] = 90;
    header.nsymbt = _metaData.symmetryDataSize;

    rewind(_file);
    if (fwrite(&header, 1, 1024, _file) == 0 ||
        (symmetryDataSize() != 0 &&
         fwrite(_symmetryData, 1, symmetryDataSize(), _file) == 0))
        REPORT_ERROR("Fail to write out an image.");

    WRITE_CAST(src, float);

    fclose(_file);
    _file = NULL;
}
