from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class Species(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    SPECIE_UNKNOWN: _ClassVar[Species]
    SPECIE_SAMPLE_NUMBER: _ClassVar[Species]
    SPECIE_TEMPERATURE: _ClassVar[Species]
    SPECIE_HUMIDITY: _ClassVar[Species]
    SPECIE_PRESSURE: _ClassVar[Species]
    SPECIE_O2_CONCENTRATION: _ClassVar[Species]
    SPECIE_H2_CONCENTRATION: _ClassVar[Species]
    SPECIE_METHANE_CONCENTRATION: _ClassVar[Species]
    SPECIE_CO2_CONCENTRATION: _ClassVar[Species]
    SPECIE_ACCELERATION: _ClassVar[Species]
    SPECIE_GYROSCOPE: _ClassVar[Species]
    SPECIE_COMPASS: _ClassVar[Species]
    SPECIE_VOLTAGE: _ClassVar[Species]
    SPECIE_CURRENT: _ClassVar[Species]
    SPECIE_POWER: _ClassVar[Species]
    SPECIE_LATITUDE: _ClassVar[Species]
    SPECIE_LONGITUDE: _ClassVar[Species]

class Units(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    UNIT_UNKNOWN: _ClassVar[Units]
    UNIT_UNITLESS: _ClassVar[Units]
    UNIT_FAHRENHEIT: _ClassVar[Units]
    UNIT_CELSIUS: _ClassVar[Units]
    UNIT_PPB: _ClassVar[Units]
    UNIT_PPM: _ClassVar[Units]
    UNIT_PERCENT: _ClassVar[Units]
    UNIT_MBAR: _ClassVar[Units]
    UNIT_PASCAL: _ClassVar[Units]
    UNIT_MILLI_G: _ClassVar[Units]
    UNIT_DEG_PER_SEC: _ClassVar[Units]
    UNIT_MILLI_T: _ClassVar[Units]
    UNIT_VOLTS: _ClassVar[Units]
    UNIT_AMPS: _ClassVar[Units]
    UNIT_WATTS: _ClassVar[Units]

class LocationType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    LOCATION_UNKNOWN: _ClassVar[LocationType]
    LOCATION_BACKPACK: _ClassVar[LocationType]
    LOCATION_MAIN: _ClassVar[LocationType]
    LOCATION_FLOW_METER: _ClassVar[LocationType]
    LOCATION_PAM: _ClassVar[LocationType]
SPECIE_UNKNOWN: Species
SPECIE_SAMPLE_NUMBER: Species
SPECIE_TEMPERATURE: Species
SPECIE_HUMIDITY: Species
SPECIE_PRESSURE: Species
SPECIE_O2_CONCENTRATION: Species
SPECIE_H2_CONCENTRATION: Species
SPECIE_METHANE_CONCENTRATION: Species
SPECIE_CO2_CONCENTRATION: Species
SPECIE_ACCELERATION: Species
SPECIE_GYROSCOPE: Species
SPECIE_COMPASS: Species
SPECIE_VOLTAGE: Species
SPECIE_CURRENT: Species
SPECIE_POWER: Species
SPECIE_LATITUDE: Species
SPECIE_LONGITUDE: Species
UNIT_UNKNOWN: Units
UNIT_UNITLESS: Units
UNIT_FAHRENHEIT: Units
UNIT_CELSIUS: Units
UNIT_PPB: Units
UNIT_PPM: Units
UNIT_PERCENT: Units
UNIT_MBAR: Units
UNIT_PASCAL: Units
UNIT_MILLI_G: Units
UNIT_DEG_PER_SEC: Units
UNIT_MILLI_T: Units
UNIT_VOLTS: Units
UNIT_AMPS: Units
UNIT_WATTS: Units
LOCATION_UNKNOWN: LocationType
LOCATION_BACKPACK: LocationType
LOCATION_MAIN: LocationType
LOCATION_FLOW_METER: LocationType
LOCATION_PAM: LocationType

class Location(_message.Message):
    __slots__ = ["type", "name"]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    type: LocationType
    name: str
    def __init__(self, type: _Optional[_Union[LocationType, str]] = ..., name: _Optional[str] = ...) -> None: ...

class Value_Float(_message.Message):
    __slots__ = ["value"]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    value: float
    def __init__(self, value: _Optional[float] = ...) -> None: ...

class Sample(_message.Message):
    __slots__ = ["species", "units", "value", "location"]
    SPECIES_FIELD_NUMBER: _ClassVar[int]
    UNITS_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    LOCATION_FIELD_NUMBER: _ClassVar[int]
    species: Species
    units: Units
    value: float
    location: _containers.RepeatedCompositeFieldContainer[Location]
    def __init__(self, species: _Optional[_Union[Species, str]] = ..., units: _Optional[_Union[Units, str]] = ..., value: _Optional[float] = ..., location: _Optional[_Iterable[_Union[Location, _Mapping]]] = ...) -> None: ...

class Record(_message.Message):
    __slots__ = ["timestamp_us", "samples"]
    TIMESTAMP_US_FIELD_NUMBER: _ClassVar[int]
    SAMPLES_FIELD_NUMBER: _ClassVar[int]
    timestamp_us: int
    samples: _containers.RepeatedCompositeFieldContainer[Sample]
    def __init__(self, timestamp_us: _Optional[int] = ..., samples: _Optional[_Iterable[_Union[Sample, _Mapping]]] = ...) -> None: ...
