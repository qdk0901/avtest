#include <stdio.h>
#include "avplayer.h"

class VideoBuffer
{
public:
	VideoBuffer() {
	}
	
	void SetBuffer(unsigned char* buffer) {
		mBuffer = buffer;
		mTotalLength = 0;
	}
	
	void AppendBuffer(unsigned char* buffer, int len) {
		memcpy(mBuffer + mTotalLength, buffer, len);
		mTotalLength += len;
	}
	
	void DisposeOneFrame(int len) {
		memmove(mBuffer, mBuffer + len, mTotalLength - len);
		mTotalLength -= len;
	}
	
	int SearchStartCode() {
		int count = 0;
		for (int i = 4; i < mTotalLength; i++) {
			switch(count) {
				case 0:
				case 1:
				case 2:
					if (mBuffer[i] == 0) {
						count++;
					} else {
						count = 0;
					}
				break;
				case 3:
					if (mBuffer[i] == 1) {
						return i - 3;
					} else {
						count = 0;
					}
			}
		}
		
		return 0;
	}
	
	unsigned char* GetBuffer() {
		return mBuffer;
	}

private:
	unsigned char* mBuffer;
	int mTotalLength;
};

#define MAX_BUFFER_SIZE (1024 * 1024)
#define BULK_SIZE 32768

int main()
{
	FILE* video_fp = fopen("/data/local/tmp/test.h264", "rb");
	unsigned char data_buffer[MAX_BUFFER_SIZE];
	
	VideoBuffer buffer;
	buffer.SetBuffer(data_buffer);
	
	AVPlayer avplayer;
	avplayer.InitVideo();
	
	while(true) {
		unsigned char data[BULK_SIZE];
		int len = fread(data, 1, BULK_SIZE, video_fp);
		if (len <= 0)
			break;
		
		buffer.AppendBuffer(data, len);
		
		while(true) {
			int i = buffer.SearchStartCode();
			if (i == 0)
				break;
			
			avplayer.FeedOneH264Frame(buffer.GetBuffer(), i);
			buffer.DisposeOneFrame(i);
		}
	}
	return NULL;	
}