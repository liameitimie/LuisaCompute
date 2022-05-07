class float2x2:
    def __init__(self, *args, **kwargs): ...
class float3x3:
    def __init__(self, *args, **kwargs): ...
class float4x4:
    def __init__(self, *args, **kwargs): ...
class int2:
    def __init__(self, *args, **kwargs): ...
class int3:
    def __init__(self, *args, **kwargs): ...
class int4:
    def __init__(self, *args, **kwargs): ...
class uint2:
    def __init__(self, *args, **kwargs): ...
class uint3:
    def __init__(self, *args, **kwargs): ...
class uint4:
    def __init__(self, *args, **kwargs): ...
class float2:
    def __init__(self, *args, **kwargs): ...
class float3:
    def __init__(self, *args, **kwargs): ...
class float4:
    def __init__(self, *args, **kwargs): ...
class bool2:
    def __init__(self, *args, **kwargs): ...
class bool3:
    def __init__(self, *args, **kwargs): ...
class bool4:
    def __init__(self, *args, **kwargs): ...
class FsPath:
    def __init__(self, *args, **kwargs): ...
class Context:
    def __init__(self, *args, **kwargs): ...
    def installed_backends(self, *args, **kwargs): ...
    def create_device(self, *args, **kwargs): ...
class Device:
    def __init__(self, *args, **kwargs): ...
    def impl(self, *args, **kwargs): ...
    def create_stream(self, *args, **kwargs): ...
    def create_accel(self, *args, **kwargs): ...
class DeviceInterface:
    def __init__(self, *args, **kwargs): ...
    def emplace_tex3d_in_bindless_array(self, *args, **kwargs): ...
    def create_mesh(self, *args, **kwargs): ...
    def create_texture(self, *args, **kwargs): ...
    def emplace_tex2d_in_bindless_array(self, *args, **kwargs): ...
    def destroy_buffer(self, *args, **kwargs): ...
    def destroy_shader(self, *args, **kwargs): ...
    def destroy_bindless_array(self, *args, **kwargs): ...
    def destroy_texture(self, *args, **kwargs): ...
    def remove_tex2d_in_bindless_array(self, *args, **kwargs): ...
    def remove_tex3d_in_bindless_array(self, *args, **kwargs): ...
    def destroy_mesh(self, *args, **kwargs): ...
    def emplace_buffer_in_bindless_array(self, *args, **kwargs): ...
    def create_bindless_array(self, *args, **kwargs): ...
    def create_buffer(self, *args, **kwargs): ...
    def remove_buffer_in_bindless_array(self, *args, **kwargs): ...
    def create_shader(self, *args, **kwargs): ...
    def is_resource_in_bindless_array(self, *args, **kwargs): ...
class Stream:
    def __init__(self, *args, **kwargs): ...
    def synchronize(self, *args, **kwargs): ...
    def add_callback(self, *args, **kwargs): ...
    def add(self, *args, **kwargs): ...
class Function:
    def __init__(self, *args, **kwargs): ...
class FunctionBuilder:
    def __init__(self, *args, **kwargs): ...
    def bindless_array_binding(self, *args, **kwargs): ...
    def buffer_binding(self, *args, **kwargs): ...
    def texture(self, *args, **kwargs): ...
    def dispatch_size(self, *args, **kwargs): ...
    def return_(self, *args, **kwargs): ...
    def accel_binding(self, *args, **kwargs): ...
    def block_id(self, *args, **kwargs): ...
    def break_(self, *args, **kwargs): ...
    def bindless_array(self, *args, **kwargs): ...
    def reference(self, *args, **kwargs): ...
    def cast(self, *args, **kwargs): ...
    def binary(self, *args, **kwargs): ...
    def loop_(self, *args, **kwargs): ...
    def member(self, *args, **kwargs): ...
    def define_callable(self, *args, **kwargs): ...
    def unary(self, *args, **kwargs): ...
    def local(self, *args, **kwargs): ...
    def literal(self, *args, **kwargs): ...
    def call(self, *args, **kwargs): ...
    def dispatch_id(self, *args, **kwargs): ...
    def accel(self, *args, **kwargs): ...
    def access(self, *args, **kwargs): ...
    def texture_binding(self, *args, **kwargs): ...
    def for_(self, *args, **kwargs): ...
    def function(self, *args, **kwargs): ...
    def thread_id(self, *args, **kwargs): ...
    def assign(self, *args, **kwargs): ...
    def set_block_size(self, *args, **kwargs): ...
    def swizzle(self, *args, **kwargs): ...
    def if_(self, *args, **kwargs): ...
    def comment_(self, *args, **kwargs): ...
    def continue_(self, *args, **kwargs): ...
    def define_kernel(self, *args, **kwargs): ...
    def buffer(self, *args, **kwargs): ...
    def argument(self, *args, **kwargs): ...
class Expression:
    def __init__(self, *args, **kwargs): ...
class LiteralExpr:
    def __init__(self, *args, **kwargs): ...
class RefExpr:
    def __init__(self, *args, **kwargs): ...
class CallExpr:
    def __init__(self, *args, **kwargs): ...
class UnaryExpr:
    def __init__(self, *args, **kwargs): ...
class BinaryExpr:
    def __init__(self, *args, **kwargs): ...
class MemberExpr:
    def __init__(self, *args, **kwargs): ...
class AccessExpr:
    def __init__(self, *args, **kwargs): ...
class CastExpr:
    def __init__(self, *args, **kwargs): ...
class ScopeStmt:
    def __init__(self, *args, **kwargs): ...
    def __exit__(self, *args, **kwargs): ...
    def __enter__(self, *args, **kwargs): ...
class IfStmt:
    def __init__(self, *args, **kwargs): ...
    def true_branch(self, *args, **kwargs): ...
    def false_branch(self, *args, **kwargs): ...
class LoopStmt:
    def __init__(self, *args, **kwargs): ...
    def body(self, *args, **kwargs): ...
class ForStmt:
    def __init__(self, *args, **kwargs): ...
    def body(self, *args, **kwargs): ...
class Type:
    def __init__(self, *args, **kwargs): ...
    def is_basic(self, *args, **kwargs): ...
    def size(self, *args, **kwargs): ...
    def is_matrix(self, *args, **kwargs): ...
    def is_texture(self, *args, **kwargs): ...
    def element(self, *args, **kwargs): ...
    def is_vector(self, *args, **kwargs): ...
    def is_bindless_array(self, *args, **kwargs): ...
    def alignment(self, *args, **kwargs): ...
    def is_buffer(self, *args, **kwargs): ...
    def description(self, *args, **kwargs): ...
    def is_array(self, *args, **kwargs): ...
    def is_structure(self, *args, **kwargs): ...
    def is_accel(self, *args, **kwargs): ...
    def is_scalar(self, *args, **kwargs): ...
    @staticmethod
    def from_(*args, **kwargs): ...
    def dimension(self, *args, **kwargs): ...
class Command:
    def __init__(self, *args, **kwargs): ...
class ShaderDispatchCommand:
    def __init__(self, *args, **kwargs): ...
    def encode_accel(self, *args, **kwargs): ...
    def set_dispatch_size(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
    def encode_buffer(self, *args, **kwargs): ...
    def encode_bindless_array(self, *args, **kwargs): ...
    def encode_uniform(self, *args, **kwargs): ...
    def encode_texture(self, *args, **kwargs): ...
class BufferUploadCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class BufferDownloadCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class BufferCopyCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class TextureUploadCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class TextureDownloadCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class TextureCopyCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class BufferToTextureCopyCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class TextureToBufferCopyCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class MeshBuildCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class AccelBuildCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class BindlessArrayUpdateCommand:
    def __init__(self, *args, **kwargs): ...
    @staticmethod
    def create(*args, **kwargs): ...
class Accel:
    def __init__(self, *args, **kwargs): ...
    def size(self, *args, **kwargs): ...
    def set_visibility_on_update(self, *args, **kwargs): ...
    def set_transform_on_update(self, *args, **kwargs): ...
    def handle(self, *args, **kwargs): ...
    def build_command(self, *args, **kwargs): ...
    def set(self, *args, **kwargs): ...
    def pop_back(self, *args, **kwargs): ...
    def emplace_back(self, *args, **kwargs): ...
class AccelModification:
    def __init__(self, *args, **kwargs): ...
    def set_mesh(self, *args, **kwargs): ...
    def set_visibility(self, *args, **kwargs): ...
    def set_transform(self, *args, **kwargs): ...
class Sampler:
    def __init__(self, *args, **kwargs): ...
def make_float2x2(*args, **kwargs) -> float2x2: ...
def make_float3x3(*args, **kwargs) -> float3x3: ...
def make_float4x4(*args, **kwargs) -> float4x4: ...
def dispatch_id(*args, **kwargs): ...
def thread_id(*args, **kwargs): ...
def block_id(*args, **kwargs): ...
def dispatch_size(*args, **kwargs): ...
def set_block_size(*args, **kwargs): ...
def make_int2(*args, **kwargs) -> int2: ...
def make_int3(*args, **kwargs) -> int3: ...
def make_int4(*args, **kwargs) -> int4: ...
def make_uint2(*args, **kwargs) -> uint2: ...
def make_uint3(*args, **kwargs) -> uint3: ...
def make_uint4(*args, **kwargs) -> uint4: ...
def make_float2(*args, **kwargs) -> float2: ...
def make_float3(*args, **kwargs) -> float3: ...
def make_float4(*args, **kwargs) -> float4: ...
def make_bool2(*args, **kwargs) -> bool2: ...
def make_bool3(*args, **kwargs) -> bool3: ...
def make_bool4(*args, **kwargs) -> bool4: ...
def log_level_verbose(*args, **kwargs): ...
def log_level_info(*args, **kwargs): ...
def log_level_warning(*args, **kwargs): ...
def log_level_error(*args, **kwargs): ...
def builder() -> FunctionBuilder: ...
def to_bytes(*args, **kwargs): ...
def pixel_storage_channel_count(*args, **kwargs): ...
def pixel_storage_to_format_int(*args, **kwargs): ...
def pixel_storage_to_format_float(*args, **kwargs): ...
def pixel_storage_size(*args, **kwargs): ...
class AccelUsageHint:
    FAST_TRACE = ...
    FAST_UPDATE = ...
    FAST_BUILD = ...
class AccelBuildRequest:
    PREFER_UPDATE = ...
    FORCE_BUILD = ...
class PixelFormat:
    R8SInt = ...
    R8UInt = ...
    R8UNorm = ...
    RG8SInt = ...
    RG8UInt = ...
    RG8UNorm = ...
    RGBA8SInt = ...
    RGBA8UInt = ...
    RGBA8UNorm = ...
    R16SInt = ...
    R16UInt = ...
    R16UNorm = ...
    RG16SInt = ...
    RG16UInt = ...
    RG16UNorm = ...
    RGBA16SInt = ...
    RGBA16UInt = ...
    RGBA16UNorm = ...
    R32SInt = ...
    R32UInt = ...
    RG32SInt = ...
    RG32UInt = ...
    RGBA32SInt = ...
    RGBA32UInt = ...
    R16F = ...
    RG16F = ...
    RGBA16F = ...
    R32F = ...
    RG32F = ...
    RGBA32F = ...
class PixelStorage:
    BYTE1 = ...
    BYTE2 = ...
    BYTE4 = ...
    SHORT1 = ...
    SHORT2 = ...
    SHORT4 = ...
    INT1 = ...
    INT2 = ...
    INT4 = ...
    HALF1 = ...
    HALF2 = ...
    HALF4 = ...
    FLOAT1 = ...
    FLOAT2 = ...
    FLOAT4 = ...
class Filter:
    POINT = ...
    LINEAR_POINT = ...
    LINEAR_LINEAR = ...
    ANISOTROPIC = ...
class Address:
    EDGE = ...
    REPEAT = ...
    MIRROR = ...
    ZERO = ...
class CastOp:
    STATIC = ...
    BITWISE = ...
class UnaryOp:
    PLUS = ...
    MINUS = ...
    NOT = ...
    BIT_NOT = ...
class BinaryOp:
    ADD = ...
    SUB = ...
    MUL = ...
    DIV = ...
    MOD = ...
    BIT_AND = ...
    BIT_OR = ...
    BIT_XOR = ...
    SHL = ...
    SHR = ...
    AND = ...
    OR = ...
    LESS = ...
    GREATER = ...
    LESS_EQUAL = ...
    GREATER_EQUAL = ...
    EQUAL = ...
    NOT_EQUAL = ...
class CallOp:
    CUSTOM = ...
    ALL = ...
    ANY = ...
    SELECT = ...
    CLAMP = ...
    LERP = ...
    STEP = ...
    ABS = ...
    MIN = ...
    MAX = ...
    CLZ = ...
    CTZ = ...
    POPCOUNT = ...
    REVERSE = ...
    ISINF = ...
    ISNAN = ...
    ACOS = ...
    ACOSH = ...
    ASIN = ...
    ASINH = ...
    ATAN = ...
    ATAN2 = ...
    ATANH = ...
    COS = ...
    COSH = ...
    SIN = ...
    SINH = ...
    TAN = ...
    TANH = ...
    EXP = ...
    EXP2 = ...
    EXP10 = ...
    LOG = ...
    LOG2 = ...
    LOG10 = ...
    POW = ...
    SQRT = ...
    RSQRT = ...
    CEIL = ...
    FLOOR = ...
    FRACT = ...
    TRUNC = ...
    ROUND = ...
    FMA = ...
    COPYSIGN = ...
    CROSS = ...
    DOT = ...
    LENGTH = ...
    LENGTH_SQUARED = ...
    NORMALIZE = ...
    FACEFORWARD = ...
    DETERMINANT = ...
    TRANSPOSE = ...
    INVERSE = ...
    SYNCHRONIZE_BLOCK = ...
    ATOMIC_EXCHANGE = ...
    ATOMIC_COMPARE_EXCHANGE = ...
    ATOMIC_FETCH_ADD = ...
    ATOMIC_FETCH_SUB = ...
    ATOMIC_FETCH_AND = ...
    ATOMIC_FETCH_OR = ...
    ATOMIC_FETCH_XOR = ...
    ATOMIC_FETCH_MIN = ...
    ATOMIC_FETCH_MAX = ...
    BUFFER_READ = ...
    BUFFER_WRITE = ...
    TEXTURE_READ = ...
    TEXTURE_WRITE = ...
    BINDLESS_TEXTURE2D_SAMPLE = ...
    BINDLESS_TEXTURE2D_SAMPLE_LEVEL = ...
    BINDLESS_TEXTURE2D_SAMPLE_GRAD = ...
    BINDLESS_TEXTURE3D_SAMPLE = ...
    BINDLESS_TEXTURE3D_SAMPLE_LEVEL = ...
    BINDLESS_TEXTURE3D_SAMPLE_GRAD = ...
    BINDLESS_TEXTURE2D_READ = ...
    BINDLESS_TEXTURE3D_READ = ...
    BINDLESS_TEXTURE2D_READ_LEVEL = ...
    BINDLESS_TEXTURE3D_READ_LEVEL = ...
    BINDLESS_TEXTURE2D_SIZE = ...
    BINDLESS_TEXTURE3D_SIZE = ...
    BINDLESS_TEXTURE2D_SIZE_LEVEL = ...
    BINDLESS_TEXTURE3D_SIZE_LEVEL = ...
    BINDLESS_BUFFER_READ = ...
    MAKE_BOOL2 = ...
    MAKE_BOOL3 = ...
    MAKE_BOOL4 = ...
    MAKE_INT2 = ...
    MAKE_INT3 = ...
    MAKE_INT4 = ...
    MAKE_UINT2 = ...
    MAKE_UINT3 = ...
    MAKE_UINT4 = ...
    MAKE_FLOAT2 = ...
    MAKE_FLOAT3 = ...
    MAKE_FLOAT4 = ...
    MAKE_FLOAT2X2 = ...
    MAKE_FLOAT3X3 = ...
    MAKE_FLOAT4X4 = ...
    ASSUME = ...
    UNREACHABLE = ...
    INSTANCE_TO_WORLD_MATRIX = ...
    TRACE_CLOSEST = ...
    TRACE_ANY = ...
