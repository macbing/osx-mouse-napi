#ifndef _MOUSE_H
#define _MOUSE_H

#include <ApplicationServices/ApplicationServices.h>
#include <napi.h>
#include <uv.h>

struct MouseEvent {
	CGFloat x;
	CGFloat y;
	CGEventType type;
};

const unsigned int BUFFER_SIZE = 10;

class Mouse : public Napi::ObjectWrap<Mouse> {
	public:
		static Napi::Object Init(Napi::Env env, Napi::Object exports);
		Mouse(const Napi::CallbackInfo& info);
		~Mouse();

		void Run();
		void Stop();
		void HandleEvent(CGEventType type, CGEventRef event);
		void HandleSend();

	private:
		Napi::FunctionReference* event_callback;
		Napi::AsyncContext* async_context;
		napi_env env_;
		uv_async_t* async;
		uv_mutex_t async_lock;
		uv_thread_t thread;
		uv_cond_t async_cond;
		CFRunLoopRef loop_ref;
		volatile bool stopped;
		MouseEvent* eventBuffer[BUFFER_SIZE];
		unsigned int readIndex;
		unsigned int writeIndex;

		Napi::Value Destroy(const Napi::CallbackInfo& info);
		Napi::Value AddRef(const Napi::CallbackInfo& info);
		Napi::Value RemoveRef(const Napi::CallbackInfo& info);

		static void OnSend(uv_async_t* handle);
		static void OnClose(uv_handle_t* handle);
};

#endif
