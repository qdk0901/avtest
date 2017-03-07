#ifndef _AV_PLAYER_H_
#define _AV_PLAYER_H_
#include <unistd.h>
#include <pthread.h>
#include <queue>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <media/ICrypto.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/MediaDefs.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/Surface.h>
#include <ui/DisplayInfo.h>
#include <media/stagefright/NativeWindowWrapper.h>

#include <media/AudioTrack.h>
#include <time.h>

#define FRAME_SIZE 32768
#define FRAME_COUNT 8

struct audio_frame
{
    unsigned char* data;
    int len;
};

using namespace android;

class AVPlayer
{
public:
	AVPlayer() {
		mVideoFrameCount = 0;
		mBeginTime = 0;
    }
	
	int InitVideo();
	int FeedOneH264Frame(unsigned char* frame, int size);
	void MakeBackground();

	sp<MediaCodec> mCodec;
	Vector<sp<ABuffer> > mBuffers[2];
	sp<SurfaceComposerClient> mComposerClient;
    sp<SurfaceControl> mControl;
	sp<SurfaceControl> mControlBG;
    sp<Surface> mSurface;
	sp<ALooper> mLooper;
	sp<AMessage> mFormat;
	
	int mWidth;
	int mHeight;
	bool mRendering;
	
	void CheckIfFormatChange();
	int RenderOneFrame();
	void RenderFrames();
	static void* VideoRenderThread(void* arg);
	
	int InitAudio(int sample_rate, int channel);
	int FeedOnePcmFrame(unsigned char* frame, int len);
	
	void Dispose();
	
private:
	AudioTrack* mAudioTrack;
	
	std::queue<struct audio_frame> mFreeQueue;
    std::queue<struct audio_frame> mDataQueue;

    void AudioQueueInit();
    bool AudioQueueBuffer(unsigned char* buffer, int len);

    void ProcessAudioData();
	bool mAudioThreadRunning;
    static void* AudioThread(void* arg);
	
	unsigned char mAudioBuffer[FRAME_SIZE * FRAME_COUNT];
	
	void* mPrivate;
	int mVideoFrameCount;
	clock_t mBeginTime;
};
#endif
