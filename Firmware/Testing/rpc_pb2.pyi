import sample_pb2 as _sample_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class FUNCTION(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    FUNC_NONE: _ClassVar[FUNCTION]
    FUNC_PING: _ClassVar[FUNCTION]
    FUNC_GET_RECORD: _ClassVar[FUNCTION]
    FUNC_POST_RECORD: _ClassVar[FUNCTION]

class ResponseType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    SUCCESS: _ClassVar[ResponseType]
    ERROR: _ClassVar[ResponseType]
FUNC_NONE: FUNCTION
FUNC_PING: FUNCTION
FUNC_GET_RECORD: FUNCTION
FUNC_POST_RECORD: FUNCTION
SUCCESS: ResponseType
ERROR: ResponseType

class InOut(_message.Message):
    __slots__ = ["error", "integer", "record"]
    ERROR_FIELD_NUMBER: _ClassVar[int]
    INTEGER_FIELD_NUMBER: _ClassVar[int]
    RECORD_FIELD_NUMBER: _ClassVar[int]
    error: Error
    integer: int
    record: _sample_pb2.Record
    def __init__(self, error: _Optional[_Union[Error, _Mapping]] = ..., integer: _Optional[int] = ..., record: _Optional[_Union[_sample_pb2.Record, _Mapping]] = ...) -> None: ...

class Request(_message.Message):
    __slots__ = ["func", "args"]
    FUNC_FIELD_NUMBER: _ClassVar[int]
    ARGS_FIELD_NUMBER: _ClassVar[int]
    func: FUNCTION
    args: InOut
    def __init__(self, func: _Optional[_Union[FUNCTION, str]] = ..., args: _Optional[_Union[InOut, _Mapping]] = ...) -> None: ...

class Cancel(_message.Message):
    __slots__ = []
    def __init__(self) -> None: ...

class Response(_message.Message):
    __slots__ = ["type", "result"]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    RESULT_FIELD_NUMBER: _ClassVar[int]
    type: ResponseType
    result: InOut
    def __init__(self, type: _Optional[_Union[ResponseType, str]] = ..., result: _Optional[_Union[InOut, _Mapping]] = ...) -> None: ...

class Error(_message.Message):
    __slots__ = ["code", "message"]
    CODE_FIELD_NUMBER: _ClassVar[int]
    MESSAGE_FIELD_NUMBER: _ClassVar[int]
    code: int
    message: str
    def __init__(self, code: _Optional[int] = ..., message: _Optional[str] = ...) -> None: ...

class Message(_message.Message):
    __slots__ = ["version", "m_id", "rpc_id", "source", "target", "request", "cancel", "response", "checksum"]
    VERSION_FIELD_NUMBER: _ClassVar[int]
    M_ID_FIELD_NUMBER: _ClassVar[int]
    RPC_ID_FIELD_NUMBER: _ClassVar[int]
    SOURCE_FIELD_NUMBER: _ClassVar[int]
    TARGET_FIELD_NUMBER: _ClassVar[int]
    REQUEST_FIELD_NUMBER: _ClassVar[int]
    CANCEL_FIELD_NUMBER: _ClassVar[int]
    RESPONSE_FIELD_NUMBER: _ClassVar[int]
    CHECKSUM_FIELD_NUMBER: _ClassVar[int]
    version: int
    m_id: int
    rpc_id: int
    source: int
    target: int
    request: Request
    cancel: Cancel
    response: Response
    checksum: int
    def __init__(self, version: _Optional[int] = ..., m_id: _Optional[int] = ..., rpc_id: _Optional[int] = ..., source: _Optional[int] = ..., target: _Optional[int] = ..., request: _Optional[_Union[Request, _Mapping]] = ..., cancel: _Optional[_Union[Cancel, _Mapping]] = ..., response: _Optional[_Union[Response, _Mapping]] = ..., checksum: _Optional[int] = ...) -> None: ...
