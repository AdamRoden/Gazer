"""Quick Stream Engine probe — lists local Tobii devices."""
import ctypes
from ctypes import CFUNCTYPE, POINTER, byref, c_char_p, c_int, c_void_p

DLL = r"C:\Program Files\Tobii\Tobii EyeX\tobii_stream_engine.dll"
dll = ctypes.CDLL(DLL)
print("DLL loaded:", DLL)

api_create = dll.tobii_api_create
api_create.argtypes = [POINTER(c_void_p), c_void_p, c_void_p]
api_create.restype = c_int

api_destroy = dll.tobii_api_destroy
api_destroy.argtypes = [c_void_p]
api_destroy.restype = c_int

error_message = dll.tobii_error_message
error_message.argtypes = [c_int]
error_message.restype = c_char_p

enum_urls = dll.tobii_enumerate_local_device_urls
enum_urls.restype = c_int

api = c_void_p()
e = api_create(byref(api), None, None)
print("api_create:", e, error_message(e))

urls = []


@CFUNCTYPE(None, c_char_p, c_void_p)
def recv(url, _ud):
    if url:
        urls.append(url.decode("utf-8", errors="replace"))


enum_urls.argtypes = [c_void_p, type(recv), c_void_p]
e = enum_urls(api, recv, None)
print("enumerate:", e, error_message(e))
print("devices:", urls if urls else "(none)")
api_destroy(api)
