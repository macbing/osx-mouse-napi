#include "mouse.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	return Mouse::Init(env, exports);
}

NODE_API_MODULE(addon, Init)
