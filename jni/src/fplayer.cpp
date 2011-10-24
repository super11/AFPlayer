#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//Fix for 	'UINT64_C' was not declared in this scope error
//@see http://code.google.com/p/ffmpegsource/issues/detail?id=11
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C" {
#include <android/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <jni.h>
}

#include "fplayer.h"

#define TAG "FPlayer.cpp"
#define INBUF_SIZE 4096

//FPlayer::FPlayer() {
//	av_register_all();
//};

static AVFormatContext *pFormatCtx;
static AVInputFormat *file_iformat;

AVDictionary **options;
AVCodecContext *codecCtx;
AVCodec *codec;
AVPacket packet;

int OUT_BUFFER_SIZE = AVCODEC_MAX_AUDIO_FRAME_SIZE; //48000 * 2 * 2 + FF_INPUT_BUFFER_PADDING_SIZE; // 48 KHz, 16 bit, 2 channels

uint8_t * pAudioOutBuffer = (uint8_t*) av_malloc(OUT_BUFFER_SIZE); //* 2 + FF_INPUT_BUFFER_PADDING_SIZE

extern "C" int start_engine() {
	AVInputFormat *p = NULL;

	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Start Engine");

	av_register_all();
	avcodec_register_all();
	avdevice_register_all();

	while (p = av_iformat_next(p)) {
		__android_log_print(ANDROID_LOG_DEBUG, TAG, p->name);
	}

	av_log_set_level(AV_LOG_VERBOSE);

	return 0;
}

extern "C" int shutdown_engine() {
	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Shutdown Engine");

	return 0;
}

extern "C" int start_audio_stream(JNIEnv *env, jobject obj, jstring filename) {

	const char url[] = "http://live-icy.gss.dr.dk:8000/Channel14_LQ.mp3";
	const char format[] = "mp3";
	int status;

	__android_log_print(ANDROID_LOG_DEBUG, TAG, "Start Audio");
	__android_log_print(ANDROID_LOG_DEBUG, TAG, url);

	if (!(file_iformat = av_find_input_format(format))) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Cannot find mp3");
		return -1;
	}

	status = avformat_open_input(&pFormatCtx, url, file_iformat, options);
	if (status != 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
				"Cannot open stream. Status: %d", status);
		return -1;
	}

	/* poulates AVFormatContex structure */
	status = av_find_stream_info(pFormatCtx);
	if (status < 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
				"Cannot read stream info. Status: %d", status);
		return -1;
	}

	if (pFormatCtx->nb_streams != 1
			&& pFormatCtx->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO) {

		__android_log_print(ANDROID_LOG_ERROR, TAG, "Sanity check failed");
		return -1;
	}

	int audioStream;
	for (int i = 0; i < pFormatCtx->nb_streams; i++) {
		__android_log_print(ANDROID_LOG_DEBUG, TAG, "Codecs %d: %s", i,
				pFormatCtx->streams[i]->codec->codec_name);
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = i;
			break;
		}
	}

	codecCtx = pFormatCtx->streams[audioStream]->codec;

	if (codecCtx == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Cannot get codec");
		return -1;
	}

	codec = avcodec_find_decoder(CODEC_ID_MP3); //avcodec_find_decoder_by_name("mp3adufloat");
	//

	if (codec == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Unsupported codec: %s", codec->name);
		return -1;
	}

	status = avcodec_open(codecCtx, codec);
	if (status < 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
				"Unsupported codec - avcodec_open: %s", codec->name);
		return -1;
	}

	jclass cls = (env)->GetObjectClass(obj);
	if (!cls) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Cannot get class");
		return -1;
	}
	jmethodID method = env->GetMethodID(cls, "streamCallback", "([B)V");
	if (!method) {
		__android_log_print(ANDROID_LOG_ERROR, TAG,
				"Cannot get callback method");
		return -1;
	}

	av_init_packet(&packet);

	int i = 0;

	while (av_read_frame(pFormatCtx, &packet) >= 0) {

		__android_log_print(ANDROID_LOG_DEBUG, TAG, "Read new frame: %d", i);
		i++;

		if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
			int data_size = OUT_BUFFER_SIZE;
			__android_log_print(
					ANDROID_LOG_DEBUG,
					TAG,
					"BitRate: %d, SampleRate: %d, DataSize: %d, FrameSize: %d, Channels: %d",
					codecCtx->bit_rate, codecCtx->sample_rate, data_size,
					codecCtx->frame_size, codecCtx->channels);
			int size = packet.size;
			if (size == 0) {
				__android_log_print(ANDROID_LOG_ERROR, TAG,
						"Packet size is null: %d", size);
			}

			__android_log_print(ANDROID_LOG_DEBUG, TAG, "Packet size: %d",
					size);

			//int decoded = 0;

			while (packet.size > 0) {

				int len = codec->decode(codecCtx, pAudioOutBuffer, &data_size, &packet);
						//avcodec_decode_audio3(codecCtx,
						//(short *) pAudioOutBuffer, &data_size, &packet);

				if (len < 0) {
					__android_log_print(ANDROID_LOG_ERROR, TAG,
							"Error while decoding");
					break;
				}

				if (data_size > 0) {
					jbyteArray bytes = env->NewByteArray(len);
					jbyte* b = env->GetByteArrayElements(bytes, NULL);
					memcpy(b, pAudioOutBuffer, len);
					env->CallVoidMethod(obj, method, bytes);
					env->ReleaseByteArrayElements(bytes, b, NULL);
					env->DeleteLocalRef(bytes);
					//packet.data = 0;
				}

				packet.size -= len;
				//packet.data += len;
				//decoded += len;

			}
			av_free_packet(&packet);
		}
	}

	free(pAudioOutBuffer);
	avcodec_close(codecCtx);
	avformat_free_context(pFormatCtx);

	return 0;
}
