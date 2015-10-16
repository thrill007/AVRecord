//Read Frame from index file
#include "stdio.h"
#include <stdint.h>
#include <string.h>
#include "libavformat/avformat.h"
class CReadFrame
{
public:
    CReadFrame();
	~CReadFrame();
	int SetIndexFilePath(const char*  pFrameIndexPath);
	int SetMediaDataFilePath(const char*   pFrameDump);
	int ReadFrame(unsigned char* pFrameData, int*  iFrameSize, uint64_t*  pillFrameTimeStamp, int  iMaxBufferSize, int* piFrameType);
	int ReadFrame(unsigned char* pFrameData, int*  iFrameSize, uint64_t*  pillFrameTimeStamp, int  iMaxBufferSize, int* piFrameType, bool is_g711);

	int Reset();
private:
	int   ParseFrameInfoLine(char*   pFrameInfoLine, int*  piFrameSize, uint64_t *  pillFrameTimeStamp, int*  piFrameType);
	int   ReadFrameData();
private:
	int   m_iCurrentFrameIndex;
	FILE*  m_pIndexFile;
	FILE*  m_pFrameDataFile;
	int   m_iTotalFrameCount;
};
