
//
// Any use of this array must be Reacquired() after a handle defrag - or after construction
template <class T>
class PODHandleArray
{
public:
    void Construct() {m_Data = 0; m_Count = 0; m_AllocSize = 0; m_NoExpand = 0; m_Handle.index = 0; m_Handle.magic = 0; }
    void FreeMem() 
    {
        m_NoExpand = 0;
        m_Count = 0;

        Handle_Free(m_Handle);
        m_Data = 0;
        m_AllocSize = 0;
    }

private:
    Handle m_Handle;
public:
    void Reacquire()
    {
        m_Data = (T*)Handle_Get(m_Handle);
    }

    const T* Data() const { return m_Data; }
    T* Data() { return m_Data; }

    S32 Push(const T& i_Data) { if (VerifyRoom(m_Count+1) == 0) return 0; memcpy(m_Data + m_Count, &i_Data, sizeof(T)); m_Count++; return 1; }
    S32 PushCount(const T* i_pData, int i_Count) { if (VerifyRoom(m_Count + i_Count) == 0) return 0; memcpy(m_Data + m_Count, i_pData, sizeof(T)*i_Count); m_Count += i_Count; return 1; }
    S32 PushEmpty() { if (VerifyRoom(m_Count+1) == 0) return 0; m_Count++; return 1; }
    S32 PushEmptyCount(int i_Count) { if (VerifyRoom(m_Count + i_Count) == 0) return 0; m_Count += i_Count; return 1; }

    void Pop() { m_Count--; }
    int Count() const { return m_Count; }
    int AllocCount() const { return m_AllocSize; }

    const T& operator[] (int i_Index) const { return m_Data[i_Index]; }
    T& operator[] (int i_Index) { return m_Data[i_Index]; }

    template <class keytype>
    int Find(const keytype& i_Key, bool (*i_SearchFunction)(const T& i_ToCheck, const keytype& i_Key))
    {
        int LocalCount = m_Count;
        for (int Index = 0; Index < LocalCount; Index++)
        {
            if (i_SearchFunction(m_Data[Index], i_Key))
            {
                return Index;
            }
        }
        return ~0;
    }

    S32 Insert(int i_Index, const T& i_ToAdd)
    {
        if (PushEmpty() == 0) return 0;
        for (int Index = m_Count-1; Index > i_Index; Index--)
        {
            m_Data[Index] = m_Data[Index-1];
        }
        m_Data[i_Index] = i_ToAdd;
    }

    void RemoveUnsorted(int i_Index)
    {
        m_Count--;
        if (m_Count != i_Index)
        {
            memcpy(m_Data + i_Index, m_Data + m_Count, sizeof(T));
        }
    }

    void Empty() { m_Count = 0; }

    const T& Top() const { return m_Data[m_Count-1]; }
    T& Top() { return m_Data[m_Count-1]; }

    void RemoveSortedCount(int i_Index, int i_Count)
    {
        m_Count-=i_Count;
        memmove(m_Data + i_Index, m_Data + i_Index + i_Count, (m_Count - i_Index) * sizeof(T));
    }

    S32 Reserve(int i_Count) { return VerifyRoom(i_Count); }

private:
    T* m_Data;
    int m_Count;
    int m_AllocSize;
    int m_NoExpand;

    S32 VerifyRoom(int i_NeededCount) 
    {
        if (m_AllocSize >= i_NeededCount) return 1;
        if (i_NeededCount < 4) i_NeededCount = 4;
        int NewSize = 2*m_AllocSize > i_NeededCount ? 2*m_AllocSize : i_NeededCount;
        int MemoryNeeded = sizeof(T)*NewSize;

        if (m_NoExpand)
        {
            // Fail
            return 0;
        }

        Handle NewHandle = Handle_Allocate(MemoryNeeded);
        T* NewData = (T*)Handle_Get(NewHandle);
        if (NewData == 0) 
        {
            m_Handle = Handle_Clear();
            return 0; // failed to allocate.
        }
        memcpy(NewData, m_Data, sizeof(T)*m_Count);
        Handle_Free(m_Handle);
        m_Handle = NewHandle;

        m_Data = NewData;
        m_AllocSize = NewSize;
        return 1;
    }
};

#undef POD_HANDLE
#undef POD_CLASS_NAME
