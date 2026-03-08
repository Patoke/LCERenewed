
class StringBuilder
{
private:
    char* m_pStr;
    S32 m_AllocLen;
    S32 m_CurrentLen;

public:
    StringBuilder()
    {
        m_pStr = 0;
        m_AllocLen = m_CurrentLen = 0;

        // Start with a null termed empty string.
        Reserve(1);
        m_pStr[0] = 0;
        m_CurrentLen = 1;
    }

    ~StringBuilder()
    {
        AIL_mem_free_lock(m_pStr);
    }

    void Pop()
    {
        if (m_CurrentLen == 1) return;
        m_CurrentLen--;
        m_pStr[m_CurrentLen-1] = 0;
    }

    void Clear()
    {
        m_pStr[0] = 0;
        m_CurrentLen = 1;
    }

    void InsertChar(S32 i_Index, char i_Char)
    {
        Reserve(m_CurrentLen + 1);

        for (S32 Mover = m_CurrentLen-1; Mover >= i_Index; Mover--)
        {
            m_pStr[Mover+1] = m_pStr[Mover];
        }

        m_pStr[i_Index] = i_Char;
        m_CurrentLen++;
    }

    void Reserve(S32 i_CharCountWithNullTerm)
    {
        if (m_AllocLen < i_CharCountWithNullTerm)
        {
            char* m_pNewStr = (char*)AIL_mem_alloc_lock(i_CharCountWithNullTerm);

            if (m_pStr) memcpy(m_pNewStr, m_pStr, m_CurrentLen);

            AIL_mem_free_lock(m_pStr);
            m_pStr = m_pNewStr;
            m_AllocLen = i_CharCountWithNullTerm;
        }
    }

    void AppendCount(char const* i_Str, S32 i_Count)
    {
        if (i_Count > (S32)strlen(i_Str))
        {
            Append(i_Str);
            return;
        }

        Reserve(m_CurrentLen + i_Count);
        strncpy(m_pStr + m_CurrentLen - 1, i_Str, i_Count);
        m_CurrentLen += i_Count;
        m_pStr[m_CurrentLen-1] = 0;
    }

    void Append(const char* i_Str)
    {
        S32 Count = (S32)strlen(i_Str);

        Reserve(m_CurrentLen + Count);

        memcpy( m_pStr + m_CurrentLen - 1, i_Str, Count + 1 ); // +1 for the null

        m_CurrentLen += Count;
    }

    void AppendFormatV(const char* i_Format, va_list i_Args)
    {
       char buf[1024];

       S32 NeededCharCount = _vsnprintf(buf, 1024, i_Format, i_Args);
       Reserve(m_CurrentLen + NeededCharCount);
       AIL_memcpy( m_pStr + m_CurrentLen - 1, buf, NeededCharCount + 1 ); // +1 for the null

       m_CurrentLen += NeededCharCount;
    }

    void AppendFormat(const char* i_Format, ...)
    {
       va_list args;
       va_start(args, i_Format);

       AppendFormatV(i_Format, args);

       va_end(args);
    }

    void Format(const char* i_Format, ...)
    {
       va_list args;
       va_start(args, i_Format);

       Clear();
       AppendFormatV(i_Format, args);

       va_end(args);
    }

    void Relinquish() 
    {
        m_pStr = 0;
        m_CurrentLen = 0;
        m_AllocLen = 0;
        Reserve(1);
        m_pStr[0] = 0;
        m_CurrentLen = 1;
    }

    S32 StrLen() const { return m_CurrentLen - 1; }

    char* Str() { return m_pStr; }
};
