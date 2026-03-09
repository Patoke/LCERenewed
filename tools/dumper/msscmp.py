# Copyright (c) 2023-present miku-666
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
# 
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
# 
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.
from dataclasses import dataclass, field
from io import SEEK_SET, BufferedReader
import pprint, datetime, subprocess
import struct, os
from typing import Callable, Any

class Logger:
    def logFn(logTarget: str | None = ...) -> Callable[[], Any]:
        def decorater(func: Callable[[Ellipsis], Any]):
            logFilename = f"{logTarget}.txt" if isinstance(logTarget, str) else f"{func.__name__}.txt"
            def caller(*args, **kwargs):
                res = func(*args, **kwargs)
                with open(logFilename, "a") as logFile:
                    print(f"[{datetime.datetime.time(datetime.datetime.utcnow())}] {repr(func.__name__)}: {pprint.pformat(res)}", file=logFile)
                return res
            return caller
        return decorater

    def logIf(condition: bool, __obj: object, targetFile: str | None = ...) -> None:
        if (condition): Logger.log(__obj, targetFile)

    def log(__obj: object, targetFile: str | None = ...) -> None:
        logFilename = f"{targetFile}.txt" if isinstance(targetFile, str) else f"{__obj}.txt"
        with open(logFilename, "a") as logFile:
            print(f"[{datetime.datetime.time(datetime.datetime.utcnow())}]: {pprint.pformat(__obj)}", file=logFile)

@dataclass(slots=True)
class BufferedDataReader:
    _stream: BufferedReader
    _endianness: str = field(default='>') # big endian

    @property
    def stream(self) -> BufferedReader:
        return self._stream

    def readInts(self, count: int) -> tuple[int, ...]:
        return struct.unpack(f"{self._endianness}{count}i", self._stream.read(count*4))

    def readInt(self) -> int:
        return self.readInts(1)[0]

    def readFloats(self, count: int) -> tuple[float, ...]:
        return struct.unpack(f"{self._endianness}{count}f", self._stream.read(count*4))

    def readFloat(self) -> float:
        return self.readFloats(1)[0]

    def readString(self) -> str:
        return self.readStringAt(self._stream.tell())

    def readStringAt(self, offset: int) -> str:
        return readAt(self._stream, offset, self.readUntil, b'\x00').decode("ASCII")

    def readUntil(self, stopper: bytes) -> bytes:
        result = bytearray()
        while self._stream.peek(1)[:1] != stopper: result.append(self._stream.read(1)[0])
        return bytes(result)

def readAt(stream: BufferedReader, offset: int, func: Callable[[Ellipsis], Any], *arg, **kwargs) -> Any:
    origin = stream.tell()
    stream.seek(offset, SEEK_SET)
    result = func(*arg, **kwargs)
    stream.seek(origin, SEEK_SET)
    return result

def convertBinka(binka_path: str, remove_original: bool = False) -> None:
    ogg_path = binka_path.rsplit('.', 1)[0] + ".ogg"
    result = subprocess.run([
        "ffmpeg", "-i", binka_path,
        "-c:a", "libvorbis",
        "-q:a", "10",
        ogg_path
    ], capture_output=True)
    if result.returncode == 0:
        if remove_original: os.remove(binka_path)
    else:
        print(f"FFmpeg failed to convert {binka_path}: {result.stderr.decode()}")

def convertBinkaDirectory(path: str, remove_original: bool = False) -> None:
    for root, _, files in os.walk(path):
        for file in files:
            if file.endswith(".binka"): convertBinka(os.path.join(root, file), remove_original)

@dataclass(frozen=True, slots=True)
class BankHeader:
    name: str
    filename: str
    mem_usage: int
    version: int

@dataclass(frozen=True, slots=True)
class BankSource:
    path_offset: int
    path: str
    file_name: str
    file_size: int
    sample_rate: int
    data_offset: int
    play_action: int
    unknown_data: dict = field(default_factory=dict)
    data: bytes = field(default_factory=bytes, repr=False)

@dataclass(slots=True)
class BankEvent:
    name: str
    raw_string_data_format: str
    sources: list[BankSource] = field(init=False, default_factory=list)
    properties: list[str] = field(init=False, default_factory=list)

@dataclass(slots=True)
class BankInfo:
    header: BankHeader = field(init=False)
    events: dict[str, BankEvent] = field(default_factory=dict)

@dataclass(slots=True)
class MsscmpParser:
    bankinfo: BankInfo = field(init=False, default_factory=BankInfo)
    verbose: bool = field(default=False)

    def process(self, stream: BufferedReader):
        #! 0x42414e4b == b'BANK' | Big Endian
        #! 0x4b4e4142 == b'KNAB' | Little Endian
        signature = stream.read(4)
        if signature not in (b'BANK', b'KNAB'):
            raise Exception("File is not a Soundbank.")
        reader = BufferedDataReader(stream, '<' if signature == b'KNAB' else '>')

        self.bankinfo.header = self.readBankHeader(reader)

        print(f"{stream.tell()=}")

        __start,       event_table_start, *__unknown_data_table_start, source_table_start  = reader.readInts(5)
        __start_count, event_count,       *__unknown_data_count,       source_count        = reader.readInts(5)

        if (stream.tell() != event_table_start):
            stream.seek(event_table_start, SEEK_SET)

        for _ in range(event_count):
            offset_event_name, offset_event_details = reader.readInts(2)
            event_name = reader.readStringAt(offset_event_name)
            raw_property_string = reader.readStringAt(offset_event_details)
            event = BankEvent(event_name, raw_property_string)

            Logger.logIf(self.verbose, event, "MSSEvents")

            event.properties = self.decodeEventProperties(raw_property_string)

            self.bankinfo.events[os.path.dirname(event_name)] = event

        if (stream.tell() != source_table_start):
            stream.seek(source_table_start, SEEK_SET)

        for _ in range(source_count):
            path_offset, info_offset = reader.readInts(2)
            source: BankSource = readAt(stream, info_offset, self.readBankSource, reader, path_offset)
            event_name: str = os.path.dirname(source.path)
            if self.bankinfo.events.get(event_name, None) is not None:
                self.bankinfo.events[event_name].sources.append(source)

            Logger.logIf(self.verbose, source, "BankSources")

    def dumpAllSources(self, path: str) -> None:
        for event in self.bankinfo.events.values():
            for source in event.sources:
                self.dumpSource(source, path)

    def dumpSource(self, bankSource: BankSource, path: str):
        full_path = f"{path}/{os.path.dirname(bankSource.path)}"
        if not os.path.exists(full_path): os.makedirs(full_path)

        binka_path = f"{path}/{bankSource.path}.binka"
        ogg_path = f"{path}/{bankSource.path}.ogg"

        with open(binka_path, "wb") as f:
            f.write(bankSource.data)

        result = subprocess.run([
            "ffmpeg", "-i", binka_path,
            "-c:a", "libvorbis",
            "-q:a", "10",
            ogg_path
        ], capture_output=True)

        if result.returncode == 0:
            os.remove(binka_path)
        else:
            print(f"FFmpeg failed to dump {binka_path}: {result.stderr.decode()}")

    def decodeEventProperties(self, raw_property_event_string: str) -> list[str]:
        properties: list[str] = raw_property_event_string.split(';')
        return properties

    def readBankHeader(self, reader: BufferedDataReader) -> BankHeader:
        # @version: Has to be 8 to match runtime
        # @mem_usage: Number of bytes required to build source info(name, file_name, file_location, file_size, ...)
        version, mem_usage, _ = reader.readInts(3)
        filename = reader.readString()
        name = reader.readStringAt(0x38)
        return BankHeader(name, filename, mem_usage, version)

    def readBankSource(self, reader: BufferedDataReader, source_path_offset: int) -> BankSource:
        current_offset = reader.stream.tell()
        unknown_data = dict()
        unknown_data["source_offset"] = current_offset

        recived_source_name_offset = reader.readInt()
        if (source_path_offset != recived_source_name_offset):
            raise Exception(f"Unexpected offset difference: Expected {source_path_offset}(0x{source_path_offset:x}) got {recived_source_name_offset}(0x{recived_source_name_offset:x})")

        path_name = reader.readStringAt(source_path_offset)
        file_name = reader.readStringAt(reader.readInt() + current_offset)
        unknown_data["0x08"] = hex(reader.readInt())
        play_action = reader.readInt()
        unknown_data["0x10"] = reader.readInt()
        sample_rate = reader.readInt()
        file_size = reader.readInt()
        unknown_data["Channels"] = reader.readInt()
        unknown_data["0x20"] = reader.readInt()
        duration_milliseconds = reader.readInt()
        unknown_data["Duration"] = f"{duration_milliseconds} ms ({duration_milliseconds/1_000} sec)"
        unknown_data["0x28"] = reader.readInt()
        unknown_data["0x2C"] = reader.readInt()
        unknown_data["0x30"] = reader.readInt()
        unknown_data["0x34"] = reader.readFloat()
        unknown_data["0x38"] = reader.readInt()

        if unknown_data["0x2C"] or unknown_data["0x30"] or unknown_data["0x38"] > 0:
            print(unknown_data)

        data_offset = int(file_name.split('*')[-1].rsplit('.')[0])

        data = readAt(reader.stream, data_offset, reader.stream.read, file_size)

        return BankSource(source_path_offset, path_name, file_name, file_size, sample_rate, data_offset, play_action, unknown_data, data)