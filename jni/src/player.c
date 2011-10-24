#include <android/log.h>
#include <jni.h>
#include "manager.h"
#include "fplayer.h"


#define TAG "Player.c"

//FManager fmanager;

JNIEXPORT void JNICALL Java_org_fpl_ffmpeg_Manager_createEngine(JNIEnv *env,
		jobject obj) {

	__android_log_write(ANDROID_LOG_DEBUG, TAG, "Create Engine");

	start_engine();
	//oc = avformat_alloc_context();
	//if (!oc) {
	//fprintf(stderr, "Memory error\n");
	//exit(1);
	//}

}

JNIEXPORT void JNICALL Java_org_fpl_ffmpeg_Manager_playStream(JNIEnv *env,
		jobject obj, jstring filename) {

	start_audio_stream(env, obj, filename);

}

JNIEXPORT void JNICALL Java_org_fpl_ffmpeg_Manager_shutdownEngine(JNIEnv *env,
		jobject obj) {
	shutdown_engine();
}
