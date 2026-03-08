
struct Handle
{
    S32 index;
    U32 magic;
};

struct HandleEntry
{
    S32 bytes;
    U32 magic;
    void* ptr;
};


inline 
Handle Handle_Clear() {Handle h; h.index = 0; h.magic = 0; return h;}

inline 
void* Handle_Get(const Handle handle)
{
    extern S32 g_MilesEvValidHandleCount;
    extern HandleEntry* g_MilesEvValidHandlePtr;

    if (handle.index == 0 || (U32)handle.index > (U32)g_MilesEvValidHandleCount) return 0;
    HandleEntry HE = g_MilesEvValidHandlePtr[-handle.index];
    if (HE.magic == handle.magic) return HE.ptr;
    return 0;
}

inline S32 Handle_Equal(const Handle a, const Handle b) { return a.index == b.index && a.magic == b.magic; }

void Handle_Free(Handle handle);
Handle Handle_Allocate(S32 i_ByteCount);
inline S32 Handle_AllocString(Handle* io_Handle, S32* o_StringLen, char const* i_String)
{
    S32 Len = (S32)strlen(i_String);
    *io_Handle = Handle_Allocate(Len + 1);
    char* S = (char*)Handle_Get(*io_Handle);
    if (S == 0)
        return 0;
    strcpy(S, i_String);
    if (o_StringLen)
        *o_StringLen = Len;
    return 1;
}
inline S32 Handle_AllocBuffer(Handle* io_Handle, void* Buffer, S32 Len)
{
    *io_Handle = Handle_Allocate(Len);
    void* B = Handle_Get(*io_Handle);
    if (B == 0)
        return 0;
    memcpy(B, Buffer, Len);
    return 1;

}

void Handle_Shutdown();
void Handle_Defrag();
void Handle_Init(void* i_Memory, S32 i_MemorySize);

void Handle_MemStats(S32* o_TotalSize, S32* o_RemainingSize);
