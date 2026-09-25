#include "mouse.h"

const char* LEFT_DOWN = "left-down";
const char* LEFT_UP = "left-up";
const char* RIGHT_DOWN = "right-down";
const char* RIGHT_UP = "right-up";
const char* MOVE = "move";
const char* LEFT_DRAG = "left-drag";
const char* RIGHT_DRAG = "right-drag";

bool IsMouseEvent(CGEventType type) {
	return type == kCGEventLeftMouseDown ||
		type == kCGEventLeftMouseUp ||
		type == kCGEventRightMouseDown ||
		type == kCGEventRightMouseUp ||
		type == kCGEventMouseMoved ||
		type == kCGEventLeftMouseDragged ||
		type == kCGEventRightMouseDragged;
}

const char* EventName(CGEventType type) {
	if (type == kCGEventLeftMouseDown) return LEFT_DOWN;
	if (type == kCGEventLeftMouseUp) return LEFT_UP;
	if (type == kCGEventRightMouseDown) return RIGHT_DOWN;
	if (type == kCGEventRightMouseUp) return RIGHT_UP;
	if (type == kCGEventMouseMoved) return MOVE;
	if (type == kCGEventLeftMouseDragged) return LEFT_DRAG;
	if (type == kCGEventRightMouseDragged) return RIGHT_DRAG;
	return nullptr;
}

void RunThread(void* arg) {
	Mouse* mouse = static_cast<Mouse*>(arg);
	mouse->Run();
}

CGEventRef OnMouseEvent(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* context) {
	Mouse* mouse = static_cast<Mouse*>(context);
	mouse->HandleEvent(type, event);
	return NULL;
}

void Mouse::OnSend(uv_async_t* handle) {
	Mouse* mouse = static_cast<Mouse*>(handle->data);
	if (mouse != nullptr) mouse->HandleSend();
}

void Mouse::OnClose(uv_handle_t* handle) {
	uv_async_t* async = reinterpret_cast<uv_async_t*>(handle);
	delete async;
}

Mouse::Mouse(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Mouse>(info) {
	Napi::Env env = info.Env();
	Napi::Function callback = info[0].As<Napi::Function>();

	for (size_t i = 0; i < BUFFER_SIZE; i++) {
		eventBuffer[i] = new MouseEvent();
	}

	readIndex = 0;
	writeIndex = 0;
	env_ = env;
	loop_ref = NULL;
	stopped = false;
	event_callback = new Napi::FunctionReference(Napi::Persistent(callback));
	async_context = new Napi::AsyncContext(env, "osx-mouse:Mouse");

	async = new uv_async_t;
	async->data = this;
	uv_async_init(uv_default_loop(), async, OnSend);
	uv_mutex_init(&async_lock);
	uv_cond_init(&async_cond);
	uv_thread_create(&thread, RunThread, this);
}

Mouse::~Mouse() {
	Stop();

	uv_mutex_destroy(&async_lock);
	uv_cond_destroy(&async_cond);

	delete event_callback;
	delete async_context;

	for (size_t i = 0; i < BUFFER_SIZE; i++) {
		delete eventBuffer[i];
	}
}

Napi::Object Mouse::Init(Napi::Env env, Napi::Object exports) {
	Napi::Function func = DefineClass(env, "Mouse", {
		InstanceMethod("destroy", &Mouse::Destroy),
		InstanceMethod("ref", &Mouse::AddRef),
		InstanceMethod("unref", &Mouse::RemoveRef)
	});

	exports.Set("Mouse", func);
	return exports;
}

void Mouse::Run() {
	CFRunLoopRef ref = CFRunLoopGetCurrent();
	CGEventMask mask = CGEventMaskBit(kCGEventLeftMouseDown) |
		CGEventMaskBit(kCGEventLeftMouseUp) |
		CGEventMaskBit(kCGEventRightMouseDown) |
		CGEventMaskBit(kCGEventRightMouseUp) |
		CGEventMaskBit(kCGEventMouseMoved) |
		CGEventMaskBit(kCGEventLeftMouseDragged) |
		CGEventMaskBit(kCGEventRightMouseDragged);

	CFMachPortRef tap = CGEventTapCreate(
		kCGHIDEventTap,
		kCGHeadInsertEventTap,
		kCGEventTapOptionListenOnly,
		mask,
		OnMouseEvent,
		this);

	CFRunLoopSourceRef source = NULL;
	if (tap != NULL) {
		source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
		CFRunLoopAddSource(ref, source, kCFRunLoopCommonModes);
		CGEventTapEnable(tap, true);
	}

	uv_mutex_lock(&async_lock);
	loop_ref = ref;
	uv_cond_signal(&async_cond);
	uv_mutex_unlock(&async_lock);

	if (tap == NULL) return;

	CFRunLoopRun();

	CGEventTapEnable(tap, false);
	CFRunLoopRemoveSource(ref, source, kCFRunLoopCommonModes);
	CFRelease(source);
	CFRelease(tap);
}

void Mouse::Stop() {
	uv_mutex_lock(&async_lock);

	if (!stopped) {
		stopped = true;
		while (loop_ref == NULL) uv_cond_wait(&async_cond, &async_lock);
		CFRunLoopRef ref = loop_ref;
		uv_async_t* handle = async;
		async = NULL;

		uv_mutex_unlock(&async_lock);

		CFRunLoopPerformBlock(ref, kCFRunLoopCommonModes, ^{
			CFRunLoopStop(CFRunLoopGetCurrent());
		});
		CFRunLoopWakeUp(ref);
		uv_thread_join(&thread);

		if (handle != NULL) {
			handle->data = NULL;
			uv_close(reinterpret_cast<uv_handle_t*>(handle), OnClose);
		}
		return;
	}

	uv_mutex_unlock(&async_lock);
}

void Mouse::HandleEvent(CGEventType type, CGEventRef e) {
	if (!IsMouseEvent(type)) return;

	CGPoint location = CGEventGetLocation(e);

	uv_mutex_lock(&async_lock);

	if (!stopped && async != NULL) {
		eventBuffer[writeIndex]->x = location.x;
		eventBuffer[writeIndex]->y = location.y;
		eventBuffer[writeIndex]->type = type;
		writeIndex = (writeIndex + 1) % BUFFER_SIZE;
		uv_async_send(async);
	}

	uv_mutex_unlock(&async_lock);
}

void Mouse::HandleSend() {
	Napi::Env env(env_);
	Napi::HandleScope scope(env);

	while (true) {
		MouseEvent e;
		uv_mutex_lock(&async_lock);
		if (stopped || readIndex == writeIndex) {
			uv_mutex_unlock(&async_lock);
			break;
		}
		e.x = eventBuffer[readIndex]->x;
		e.y = eventBuffer[readIndex]->y;
		e.type = eventBuffer[readIndex]->type;
		readIndex = (readIndex + 1) % BUFFER_SIZE;
		uv_mutex_unlock(&async_lock);

		const char* name = EventName(e.type);
		if (name == nullptr) continue;

		event_callback->Value().MakeCallback(
			env.Global(),
			{
				Napi::String::New(env, name),
				Napi::Number::New(env, static_cast<double>(e.x)),
				Napi::Number::New(env, static_cast<double>(e.y))
			},
			*async_context);
	}
}

Napi::Value Mouse::Destroy(const Napi::CallbackInfo& info) {
	Stop();
	return info.Env().Undefined();
}

Napi::Value Mouse::AddRef(const Napi::CallbackInfo& info) {
	uv_mutex_lock(&async_lock);
	uv_async_t* handle = async;
	uv_mutex_unlock(&async_lock);

	if (handle != NULL && !uv_is_closing(reinterpret_cast<uv_handle_t*>(handle))) {
		uv_ref(reinterpret_cast<uv_handle_t*>(handle));
	}
	return info.Env().Undefined();
}

Napi::Value Mouse::RemoveRef(const Napi::CallbackInfo& info) {
	uv_mutex_lock(&async_lock);
	uv_async_t* handle = async;
	uv_mutex_unlock(&async_lock);

	if (handle != NULL && !uv_is_closing(reinterpret_cast<uv_handle_t*>(handle))) {
		uv_unref(reinterpret_cast<uv_handle_t*>(handle));
	}
	return info.Env().Undefined();
}
