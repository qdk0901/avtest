#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <android/native_window.h>

#include "avplayer.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

int AVPlayer::InitVideo()
{
	mWidth = SCREEN_WIDTH;
	mHeight = SCREEN_HEIGHT;
	
	mRendering = true;
	
	ProcessState::self()->startThreadPool();

    DataSource::RegisterDefaultSniffers();
	
	mFormat = new AMessage;
	
	mLooper = new ALooper;
	mLooper->start();
	
    mComposerClient = new SurfaceComposerClient;
    CHECK_EQ(mComposerClient->initCheck(), (status_t)OK);
	
	mControl = mComposerClient->createSurface(
		String8("A Surface"),
		mWidth,
		mHeight,
		PIXEL_FORMAT_RGB_565,
		0);
		
	CHECK(mControl != NULL);
    CHECK(mControl->isValid());

	SurfaceComposerClient::openGlobalTransaction();
    CHECK_EQ(mControl->setLayer(INT_MAX), (status_t)OK);
    CHECK_EQ(mControl->show(), (status_t)OK);
    SurfaceComposerClient::closeGlobalTransaction();
	
    mSurface = mControl->getSurface();
    CHECK(mSurface != NULL);
	
	MakeBackground();
	
	mCodec = MediaCodec::CreateByType(
                mLooper, "video/avc", false);
    
    sp<AMessage> format = new AMessage;
    format->setString("mime", "video/avc");
    format->setInt32("width", mWidth);
    format->setInt32("height", mHeight);

	
	sp<NativeWindowWrapper> window = new NativeWindowWrapper(mSurface);
	
    mCodec->configure(
                format,
                window->getSurfaceTextureClient(),
                NULL,
                0);
				
    mCodec->start();
	
	int err = mCodec->getInputBuffers(&mBuffers[0]);
    CHECK_EQ(err, (status_t)OK);
    
    err = mCodec->getOutputBuffers(&mBuffers[1]);
    CHECK_EQ(err, (status_t)OK);

	pthread_t tid;
    pthread_create(&tid, NULL, VideoRenderThread, this);	
	
    return 0;
}

void AVPlayer::MakeBackground()
{
	mControlBG = mComposerClient->createSurface(
		String8("A Surface"),
		mWidth,
		mHeight,
		PIXEL_FORMAT_RGB_565,
		0);
	
	CHECK(mControlBG != NULL);
    CHECK(mControlBG->isValid());
	
	SurfaceComposerClient::openGlobalTransaction();
	CHECK_EQ(mControlBG->setLayer(INT_MAX - 1), (status_t)OK);
    CHECK_EQ(mControlBG->show(), (status_t)OK);
    SurfaceComposerClient::closeGlobalTransaction();
	
	sp<Surface> service = mControlBG->getSurface();
	
	ANativeWindow_Buffer ob;
    service->lock(&ob, NULL);
    service->unlockAndPost();
}

void AVPlayer::CheckIfFormatChange()
{
	mCodec->getOutputFormat(&mFormat);
		
	int width, height;
	if (mFormat->findInt32("width", &width) &&
		mFormat->findInt32("height", &height)) {
		float scale_x = (SCREEN_WIDTH + 0.0) / width;
		float scale_y = (SCREEN_HEIGHT + 0.0) / height;
		float scale = (scale_x < scale_y) ? scale_x : scale_y;
		
		scale = (scale > 1) ? 1 : scale;
		
		if (scale < 1) {
			int new_width = width * scale;
			int new_height = height * scale;
			
			new_width = (new_width > SCREEN_WIDTH) ? SCREEN_WIDTH : new_width;
			new_height = (new_height > SCREEN_HEIGHT) ? SCREEN_HEIGHT : new_height;
			
			width = new_width;
			height = new_height;
		}
		
		if (width > SCREEN_WIDTH)
			width = SCREEN_WIDTH;
		
		if (height > SCREEN_HEIGHT)
			height = SCREEN_HEIGHT;
		
		if (width != mWidth || height != mHeight) {
			mWidth = width;
			mHeight = height;
			
			int x = (SCREEN_WIDTH - width) / 2;
			int y = (SCREEN_HEIGHT - height) / 2;
			
			SurfaceComposerClient::openGlobalTransaction();
			mControl->setSize(width, height);
			mControl->setPosition(x, y);
			SurfaceComposerClient::closeGlobalTransaction();
		}
	}	
}

void AVPlayer::RenderFrames()
{
	size_t index, offset, size;
	int64_t pts;
	uint32_t flags;
	
	int err;
	
	do {
		CheckIfFormatChange();
		
		err = mCodec->dequeueOutputBuffer(
			&index, &offset, &size, &pts, &flags);

		if (err == OK) {
			mCodec->renderOutputBufferAndRelease(
					index);
			
			mVideoFrameCount++;
			if (mBeginTime == 0) {
				mBeginTime = clock();
			} else {
				float fps = mVideoFrameCount / (float(clock() - mBeginTime) / CLOCKS_PER_SEC);
				printf("### %f\n", fps);
			}
		}
	} while(err == OK
                || err == INFO_FORMAT_CHANGED
                || err == INFO_OUTPUT_BUFFERS_CHANGED);
}

void* AVPlayer::VideoRenderThread(void* arg)
{
	AVPlayer* player = (AVPlayer*)arg;

    while(player->mRendering) {
        player->RenderFrames();
    }
    
    return NULL;	
}

int AVPlayer::FeedOneH264Frame(unsigned char* frame, int size)
{
	size_t index;

	int err = mCodec->dequeueInputBuffer(&index, -1ll);
	
	CHECK_EQ(err, (status_t)OK);
	
	const sp<ABuffer> &dst = mBuffers[0].itemAt(index);

	CHECK_LE(size, dst->capacity());
	
	dst->setRange(0, size);
	memcpy(dst->data(), frame, size);
	
	err = mCodec->queueInputBuffer(
					index,
					0,
					size,
					0ll,
					0);
	return err;
}
	
int AVPlayer::InitAudio(int sample_rate, int channel)
{
	size_t frameCount = 0;
	
	AudioTrack::getMinFrameCount(&frameCount, AUDIO_STREAM_MUSIC, 48000);
	
    if (mAudioTrack) {
        mAudioTrack = NULL;
    }

    mAudioTrack = new AudioTrack(AUDIO_STREAM_MUSIC, 48000, 
                      AUDIO_FORMAT_PCM_16_BIT, audio_channel_out_mask_from_count(2), 
                      frameCount, AUDIO_OUTPUT_FLAG_NONE);

    if (mAudioTrack->initCheck() != OK) {
        mAudioTrack = NULL;
        return false;
    }
	
	AudioQueueInit();
	
	pthread_t tid;
    pthread_create(&tid, NULL, AudioThread, this);
	return 0;
}

int AVPlayer::FeedOnePcmFrame(unsigned char* frame, int len)
{
	AudioQueueBuffer(frame, len);
	return 0;
}

bool AVPlayer::AudioQueueBuffer(unsigned char* buffer, int len)
{
    if (mFreeQueue.empty())
        return false;

    struct audio_frame frame = mFreeQueue.front();
    mFreeQueue.pop();

    memcpy(frame.data, buffer, len);
    frame.len = len;

    mDataQueue.push(frame);
    return true;
}

void AVPlayer::AudioQueueInit()
{
    for (int i = 0; i < FRAME_COUNT; i++) {
        struct audio_frame frame;
        frame.data = &(mAudioBuffer[i * FRAME_SIZE]);
        frame.len = FRAME_SIZE;
        mFreeQueue.push(frame);    
    }
}

void AVPlayer::ProcessAudioData()
{
	mAudioThreadRunning = true;
	
    do {
        if (mDataQueue.empty()) {
            usleep(1 * 1000);
            continue;        
        }

        struct audio_frame frame = mDataQueue.front();
        mDataQueue.pop();


		if (mAudioTrack != NULL) {
			if (mAudioTrack->stopped()) {
				mAudioTrack->start();
			}
			
			mAudioTrack->write(frame.data, frame.len);
		}
		
        mFreeQueue.push(frame);
    } while(mAudioThreadRunning);    
}

void AVPlayer::Dispose()
{
	mAudioThreadRunning = false;
	mCodec->stop();
	mCodec->reset();
	mCodec->release();
	mLooper->stop();
	
	
	mRendering = false;
	SurfaceComposerClient::openGlobalTransaction();
    CHECK_EQ(mControl->hide(), (status_t)OK);
	CHECK_EQ(mControlBG->hide(), (status_t)OK);
    SurfaceComposerClient::closeGlobalTransaction();	
	
	mComposerClient->dispose();
	mControl->clear();
	mControlBG->clear();


	if (mAudioTrack != NULL) {
		mAudioTrack->stop();
	}
	mAudioThreadRunning = false;
}

void* AVPlayer::AudioThread(void* arg)
{
    AVPlayer* avplayer = (AVPlayer*)arg;
    avplayer->ProcessAudioData();
    return NULL;
}
