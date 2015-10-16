#include "ReadFrame.h"
#include "string.h"

CReadFrame::CReadFrame()
{
    m_iCurrentFrameIndex = 0;
    m_pIndexFile = NULL;
    m_pFrameDataFile = NULL;
	m_iTotalFrameCount = 0;
}

CReadFrame::~CReadFrame()
{
	if (m_pFrameDataFile != NULL)
    {
		fclose(m_pFrameDataFile);
    }

	if (m_pIndexFile != NULL)
	{
		fclose(m_pIndexFile);
	}
}

int CReadFrame::SetIndexFilePath(const char*  pFrameIndexPath)
{
	if (pFrameIndexPath != NULL)
	{
		m_pIndexFile = fopen(pFrameIndexPath, "rb");
	}

	if (m_pIndexFile == NULL)
	{
		return 1;
	}

	fseek(m_pIndexFile, 0, SEEK_SET);
	return 0;
}

int CReadFrame::SetMediaDataFilePath(const char*   pFrameDump)
{
	if (pFrameDump != NULL)
	{
		m_pFrameDataFile = fopen(pFrameDump, "rb");
	}

	if (m_pFrameDataFile == NULL)
	{
		return 1;
	}

	fseek(m_pFrameDataFile, 0, SEEK_SET);
	return 0;
}
FILE*  m_pIndexFile;

int CReadFrame::ReadFrame(unsigned char* pFrameData, int*  piFrameSize, uint64_t*  pillFrameTimeStamp, int  piMaxBufferSize, int* piFrameType)
{
	char*  pLineEnd = NULL;
	int   iRet = 0;
	int   iReadCount = 0;
	int   iFrameSize = 0;
	uint64_t   iFrameTimeStamp = 0;
	int   iFrameType = 0;
	char  strLine[256] = {0};

	if (m_pIndexFile != NULL)
	{
		pLineEnd = fgets(strLine, 256, m_pIndexFile);
		if (pLineEnd == NULL)
		{
			return 1;
		}
	}

	if (strlen(strLine) != 0)
	{
		iRet = ParseFrameInfoLine(strLine, &iFrameSize, &iFrameTimeStamp, &iFrameType);
	}

	if (iRet != 0)
	{
		return 1;
	}

	iReadCount = fread(pFrameData+iReadCount, 1, iFrameSize-iReadCount, m_pFrameDataFile);
	while (iReadCount < iFrameSize)
	{
		iReadCount+= fread(pFrameData + iReadCount, 1, iFrameSize - iReadCount, m_pFrameDataFile);
	}

	*piFrameSize = iFrameSize;
	*pillFrameTimeStamp = iFrameTimeStamp;
	*piFrameType = iFrameType;

	return 0;
}

int CReadFrame::ReadFrame(unsigned char* pFrameData, int*  piFrameSize, uint64_t*  pillFrameTimeStamp, int  piMaxBufferSize, int* piFrameType, bool is_g711)
{
	int   iReadCount = 0;
	uint64_t   iFrameTimeStamp = AV_NOPTS_VALUE;
	int   iFrameSize = 1024;
	int   iFrameType = 0;

	iReadCount = fread(pFrameData, 1, iFrameSize-iReadCount, m_pFrameDataFile);
	printf("read count:%d\n", iReadCount);
//	while (iReadCount < iFrameSize)
//	{
//		iReadCount+= fread(pFrameData + iReadCount, 1, iFrameSize - iReadCount, m_pFrameDataFile);
//	}

	*piFrameSize = iFrameSize;
	*pillFrameTimeStamp = iFrameTimeStamp;
	*piFrameType = iFrameType;
	if (iReadCount == 0)
		return 1;
	else
		return 0;
}

int CReadFrame::Reset()
{
	return 0;
}

int   CReadFrame::ParseFrameInfoLine(char*   pFrameInfoLine, int*  piFrameSize, uint64_t*  pillFrameTimeStamp, int*  piFrameType)
{
	char*   pBread = NULL;
	uint64_t     iTimeStamp = 0;
	int     iFrameSize = 0;
	int     iFrameType = 0;
	pBread = strstr(pFrameInfoLine, "Time:");
	if (pBread != NULL)
    {
		pBread += strlen("Time:");
		sscanf(pBread, "%lld", &iTimeStamp);
    }

	pBread = strstr(pFrameInfoLine, "data size:");
	if (pBread != NULL)
	{
		pBread += strlen("data size:");
		sscanf(pBread, "%d", &iFrameSize);
	}

	pBread = strstr(pFrameInfoLine, "flag:");
	if (pBread != NULL)
	{
		pBread += strlen("flag:");
		sscanf(pBread, "%d", &iFrameType);
	}



	*piFrameSize = iFrameSize;
	*pillFrameTimeStamp = iTimeStamp;
	*piFrameType = iFrameType;
	return 0;
}

int   CReadFrame::ReadFrameData()
{
	return 0;
}
