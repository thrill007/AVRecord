//Read Frame from index file
#include "stdio.h"
#include <stdint.h>
#include <string.h>

class CReadFrame
{
public:
    CReadFrame();
	~CReadFrame();
	int SetIndexFilePath(char*  pFrameIndexPath);
	int SetMediaDataFilePath(char*   pFrameDump);
	int ReadFrame(unsigned char* pFrameData, int*  iFrameSize, uint64_t*  pillFrameTimeStamp, int  iMaxBufferSize, int* piFrameType);
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
