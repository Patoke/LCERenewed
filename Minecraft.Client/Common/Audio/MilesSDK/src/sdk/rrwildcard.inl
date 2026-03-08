// happy fast wild card matcher code:

// TO USE this file, include it in your file, and then define a funcion called char_match 
//   to compare the individual characers.
//   this function must handle '?' (if you want them), / vs \ compares, and case insenstivity.
//   a sample filename compare function is:
//   
//   static S32 char_match( char n, char w )
//   {
//     if ( w == '?' ) return 1;   // ? match anything
//     if ( ( n == '/' ) && ( w == '\\' ) ) return 1;  
//     if ( ( n == '\\' ) && ( w == '/' ) ) return 1;
//   
//     if ( ( n >= 'a' ) && ( n <= 'z' ) ) n = n - 'a' + 'A';
//     if ( ( w >= 'a' ) && ( w <= 'z' ) ) w = w - 'a' + 'A';
//   
//     return( n == w );
//   }


// NOW, you are ready to call the matching functions:

//   first, wild_card_match will compare one string to one wildcard (can't be lists of strings on either side)


// FOR fancier (and faster) compares, you generate parse data first, and then call the real functions

// use parse_string_list to parse out multiple strings into an internal list. you can specify the 
//    separator characer (comma or semi-colans, for example), and you can specify a terminator
//    character (like cr, or a space).  if you specify 0xffffffff for the length, we assume
//    it's a zero terminated string (this means you can use the terminator value for another
//    termintor type). 

// use parse_wildcar_query to parse out a query into an acceleration buffer. you can again
//    specify the separator character and the terminator.  len also supports the 0xffffffff
//    value, when you don't know the length.  
//  Queries:
//    str1,str2,str3 is any of the three,  str1,+str2,str3 is str1 or str3 and str2
//    str1,-str2,str3 is str1 or str3, and not str2
//    you can also use paranthesis "(str1,str2),+(str3,str4),-str5"


// ONCE you have the parsed data, you can use:
  
//    if you have a single wildcard match (not a query), you can use wild_card_match_name_list
//       to compare a wildcard to a list of parsed names (from parse_string_list)

//    to do a full query (not just wildcards), you use wild_card_match_list_to_query to 
//       compare a list of parsed names to a list of parsed wildcards (this is very fast and
//       is usually what you want, even with simple matches - especially when you can pre-parse
//       one side or the other.

//    finally, you can take a parsed query and convert it back into a query string with
//      unparse_wild_card_match_query.  this is useful for debugging and syntax checking


// takes about as long as a strlen
// uses ( ( 4 bytes * ( numb of substrings ) ) + 2 bytes )
static S32 parse_string_list( void * out, char const * str, U32 len, char separator, char terminator );


// takes about as long as 3 strlens
// uses ( ( 6 bytes * ( numb of expressions ) ) + ( ( 6 bytes * ( numb of logical ops ) ) + ( 2 bytes * ( numb of *'s + 1 ) ) )
static S32 parse_wildcard_query( void * out, char const * str, U32 len, char separator, char terminator );



// matches one names (parsed into strs) against one wild card string (?s and *s only), 
//   wild_counts is an optional accelerator table (built during parse_query)
static S32 wild_card_match( char const * n, char const * ne, char const * w, char const * we, U16 const * wild_counts );



// matches a list of names (parsed into strs), against one wild card string (?s and *s only), 
//   wild_counts is an optional accelerator table (built during parse_query)
static S32 wild_card_match_name_list( void const * parsed_names, char const * n, char const * w, char const * we, U16 const * wild_counts );


// matches a list of names to a parsed query
static S32 wild_card_match_list_to_query( void const * parsed_names, char const * names, 
                                          void const * parsed_query, char const * query );

// turns a parsed query back into a string (good for debugging or simplification)
static char * unparse_wild_card_match_query( char * out, void const * parsed_query, char const * query );


//=====================================================================================================================


static S32 wild_card_match( char const * n, char const * ne, char const * w, char const * we, U16 const * wild_counts )
{
  // empty queries match everything
  if ( w == we )
    return 1;
    
  // if we have the wild count accelerator, check for the fixed end portion
  if ( wild_counts ) 
  {
    S32 count = *wild_counts++;
    if ( count )
    {
      // we have a static bit on the end - check it first
      if ( ( ne - n ) < count )
        return 0;
      while ( count )
      {
        --we;
        --ne;
  
        if ( *we != *ne )
        {
          if ( !char_match( *ne, *we ) )
            return( 0 );
        }
        --count;
      }
    }
  }

 restart:
  for(;;)
  {
    if ( w == we )
    {
      if ( n == ne )
        return 1;
      return 0;
    }
    else if ( n == ne )
    {
      // we've hit the end of a string, if we are at the end of
      //   the wild card too (or do we just have *'s to go?),
      //   then we have a match, otherwise, it's a mismatch
      if ( wild_counts )
      {
        if ( w[ 0 ] == '*' ) // we know we have at least one wildcard, because we already checked the end above
          if ( *wild_counts == 0 )
            return 1;
        return 0;
      }
      else
      {
        do
        {
          if ( w[ 0 ] != '*' )
            return( 0 );
          ++w;
        } while ( w != we );
        return( 1 );
      }
    }
    else if ( ( *w ) == '*' )
    {
      char const * we_fixed;
      
      // if all that is left is *'s, then we match
      if ( ( wild_counts ) && ( *wild_counts == 0 ) )
        return 1;
        
      // skip multiple *'s in a row
      do
      {
        ++w;
      } while ( ( w != we ) && ( w[ 0 ] == '*' ) );
                  
      // find the fixed string span after the *
      if ( wild_counts )
      {
        we_fixed = w + *wild_counts++;
      }
      else
      {
        we_fixed = w;
        while ( we_fixed != we )
        {
          if ( we_fixed[ 0 ] == '*' )
            break;
          ++we_fixed;
        }
      }

      // does the fixed part go all the way to the end?
      if ( we_fixed == we )
      {
        // if we don't have enough trailing characters, then fail
        if ( ( ne - n ) < ( we - w ) )
          return 0;

        // there was a trailing piece that is all at the end (*.exe, for example)
        //   in this case, we compare the ends of the strings
        while ( w != we )
        {
          --we;
          --ne;

          if ( *we != *ne )
          {
            if ( !char_match( *ne, *we ) )
              return( 0 );
          }
        }
        return 1;
      }
      
      // if we get here, the fixed portion isn't the end of the string (*fixed*blah or similar)   
      // scan for first w to we_fixed inside n
      do
      {
        // does the character in the name currently match the first wild char
        if ( ( *n == *w ) || ( char_match( *n, *w ) ) )
        {
          // yup, let's see if the entire fixed portion of the wild matches
          char const * n_cur = n + 1;
          char const * w_cur = w + 1;
          for(;;)
          {
            // are we at the end of the fixed portion of the string? call back to the normal wild matcher
            if ( w_cur == we_fixed )
            {
              n = n_cur;
              w = we_fixed;
              goto restart;
            }
            
            // if we hit the end of the name, before the end of the wild, we don't match
            if ( n_cur == ne )
              return 0;
              
            // continue checking the fixed portion
            if ( *n_cur != *w_cur )
            {
              if ( !char_match( *n_cur, *w_cur ) )
                break;
            }
            ++n_cur;
            ++w_cur;
          }        
        }
        ++n;
      } while ( n != ne );  
      
      return 0;
    }
    else
    {
      if ( *w != *n )
      {
        if ( !char_match( *n, *w ) )
          return( 0 );
      }
      ++n;
      ++w;
    }
  }
  // won't get here
}

// matches a list of names (parsed into strs), against one wild card string (?s and *s only), 
//   wild_counts is an optional accelerator table (built during parse_query)
static S32 wild_card_match_name_list( void const * parsed_names, char const * n, char const * w, char const * we, U16 const * wild_counts )
{
  U32 num = ( (U16 const *) parsed_names )[ 0 ];

  U16 const * s = ( (U16 const *) parsed_names ) + 1;
  while( num )
  {
    n += s[ 0 ];
    if ( wild_card_match( n, n + s[ 1 ], w, we, wild_counts ) )
      return 1;
    s += 2;
    --num;
  }
 
  return 0;
}

// matches a list of names to a parsed query
static S32 wild_card_match_list_to_query( void const * parsed_names, char const * names, 
                                          void const * parsed_query, char const * query )
{                                          
  U16 const * qrys = (U16 const *) parsed_query;
  static U8 matches[ 3 ] = { 0, 1, 1 }; //ands, nots, ors
  static U8 returns[ 3 ] = { 0, 0, 1 };
  S32 logi;
 
  for( logi = 0 ; logi < 3 ; logi++ )
  {
    // query ands, nots, then ors
    U32 next = qrys[ logi ];
    char const * q = query;
    while( next )
    {
      U32 ofs = next & 0x7fff;
      q += qrys[ ofs + 1 ];
      if ( next & 0x8000 ) // hit an open paranthesis?
      {
        if ( wild_card_match_list_to_query( parsed_names, names, &qrys[ ofs + 3 ], q ) == matches[ logi ] )
          return returns[ logi ];
      }
      else
      {
        if ( wild_card_match_name_list( parsed_names, names, q, q + qrys[ ofs + 2 ], &qrys[ ofs + 3 ] ) == matches[ logi ] )
          return returns[ logi ];
      }
      next = qrys[ ofs ];
    }
  }
 
  if ( qrys[ 2 ] )
    return 0;
  
  return 1;
}  

// turns a parsed query back into a string (good for debugging or simplification)
static char * unparse_wild_card_match_query( char * out, void const * parsed_query, char const * query )
{                                          
  U16 const * qrys = (U16 const *) parsed_query;
  char * o = out;
  S32 logi;
  
  for( logi = 0 ; logi < 3 ; logi++ )
  {
    // query ands, nots, then ors
    U32 next = qrys[ logi ];
    char const * q = query;
    while( next )
    {
      U32 ofs = next & 0x7fff;
      q += qrys[ ofs + 1 ];
      if ( logi == 0 ) *o++ = '+'; else if ( logi == 1 ) *o++ = '-';
      if ( next & 0x8000 ) // hit an open paranthesis?
      {
        *o++ = '(';
        o = unparse_wild_card_match_query( o, &qrys[ ofs + 3 ], q );
        *o++ = ')';
      }
      else
      {
        U32 n = qrys[ ofs + 2 ];
        char const * cq = q;
        while( n )
        {
          *o++ = *cq++;
          --n;
        }
      }
      *o++ = ',';
      next = qrys[ ofs ];
    }
  }

  // kill trailing comma 
  if ( qrys[ 0 ] | qrys[ 1 ] | qrys[ 2 ] )
    --o;
 
  *o = 0;
  return o;
}  


// uses ( ( 4 bytes * ( numb of substrings ) ) + 2 bytes )
static S32 parse_string_list( void * out, char const * str, U32 len, char separator, char terminator )
{
  U16 * o;
  char const * s = str;
  char const * l = str;
  char const * se = str + len;
  char extra_term = separator; // this is a trick. if len == 0xffffffff, then extra_term is set to null
                               //   (to detect end of string), but if we have a length, we set the extra
                               //   terminating character to the normal separator character, which,
                               //   since we check for the extra_term *after* checking for the separator,
                               //   means the extra_term will never trigger (this saves an and clause 
                               //   in the inner loop)
  
  if ( len == 0xffffffff )  // if we don't know the length, set the extra_term to zero to detect end of string
    extra_term = 0;         //   (if they set a specific len, a zero character doesn't terminate)
  
  o = ( (U16*) out ) + 1;
    
  for(;;)
  {
    char const * cs = s;
    while ( (UINTa)cs != (UINTa)se )
    {
      if ( ( cs[ 0 ] == separator ) || ( cs[ 0 ] == extra_term ) || ( cs[ 0 ] == terminator ) )
        break;
      ++cs;
    }
    
    // store the extents
    o[ 0 ] = (U16) ( s - l );
    o[ 1 ] = (U16) ( cs - s );
    o += 2;
    l = s;
    
    if ( cs == se )
      break;
  
    s = cs;
    
    // are we at the end?
    if ( s[ 0 ] == separator )
      ++s;
    else
      if ( ( s[ 0 ] == terminator ) || ( s[ 0 ] == extra_term ) )
        break;
  }
  
  // store the final count
  ( (U16*) out )[ 0 ] = (U16) ( ( o - ( (U16*) out ) - 1 ) / 2 );

  return (S32) ( ( (char*) o ) - ( (char*) out ) );
}


static void internal_parse_wildcard_query( S32 level, U16 ** outp, char const * * strp, char const * se, char extra_term, char separator, char terminator )
{
  U16 * out = *outp;
  char const * str = *strp;
  char const * s = str;
  U16 * o = out;
    
  typedef struct
  {
    U16 * prev;
    char const * last;
  } logical;
  
  logical ands;
  logical nots;
  logical ors;
  
  // todo handle empty str
  
  ands.prev = out;
  ands.last = s;
  nots.prev = out + 1;
  nots.last = s;
  ors .prev = out + 2;
  ors .last = s;
  
  o = out + 3;
  
  if ( s == se )
  {
    out[ 0 ] = 0; out[ 1 ] = 0; out[ 2 ] = 0;
    *outp = o;
    return;
  }
  
  for(;;)
  {
    char const * cs = s;
    U16 * star_lens = o + 4;
    char const * last_star = 0;
    logical * logi = &ors;
    char ch = cs[ 0 ];
    
    // open parenthesis 
    if ( ch == '(' ) 
    {
     do_para: 
      ++cs;
      ++s;
      --star_lens;

      // link in this parenthesis
      logi->prev[ 0 ] = (U16) ( 0x8000 | ( o - out ) );
      // recurse to get interior
      internal_parse_wildcard_query( level + 1, &star_lens, &cs, se, extra_term, separator, terminator );
    }
    else 
    {    
      // plus or minus? eat it, and then check for open parenthesis 
      if ( ch == '+' )
      {
        logi = &ands;
        goto do_logi;
      }
      else if ( ch == '-' )
      {
        logi = &nots;
       do_logi:
        ++cs;
        ++s;
        ch = cs[ 0 ];
        if ( ( cs != se ) && ( cs[ 0 ] == '(' ) )
          goto do_para;
      }
    
      // find the end of the next substring 
      while ( cs != se )
      {
        ch = cs[ 0 ];
  
        // keep track of the lengths after the *s
        if ( ch == '*' )
        {
          // if we had a previous open *, save the post-fixed-string length
          //   this will alway fail on the first *, leaving a zero in the first
          //   position
          if ( last_star )
            *star_lens++ = (U16) ( cs - last_star );
          
          // kill runs of *s
          do
          {
            ++cs;
            if ( cs == se )
            { 
              last_star = cs;
              goto end_str;
            }
            ch = cs[ 0 ];
          } while( ch == '*' );
              
          last_star = cs;
        }
        
        if ( ( ch == ')' ) || ( ch == separator ) || ( ch == extra_term ) || ( ch == terminator ) )
          break;
        ++cs;
      }
  
      // if we had a *, handle the cleanup
      if ( last_star )
      {
       end_str: 
        // last star always gets a zero final length (since we pre-check the terminator portion)
        *star_lens++ = 0;
      }
      else
      {
        // if never saw a star, then the whole string is the fixed portion
        last_star = s;
      }
    
      // when we have a final fixed string (.ext in *.ext), we actually store it at the 
      //   front of the parse so that we can check it first (*.ext is common)
      //   and then we mark the final star as having a zero post_string len
      //   (since we will have already checked it)
      o[ 3 ] = (U16) ( cs - last_star );

      // link in this op
      logi->prev[ 0 ] = (U16) ( o - out );
    }   
    
    // update prev link to this one
    logi->prev = o;
    // save the string delta from previous string offset
    o[ 1 ] = (U16) ( s - logi->last );
    logi->last = s;
    // save the string length
    o[ 2 ] = (U16) ( cs - s );
    // advance the output
    o = star_lens;
    
    s = cs;
    if ( s == se )
      break;
  
    // are we done?
    if ( s[ 0 ] == separator )
      ++s;
    else
    {
      if ( s == se )
        break;
      if ( s[ 0 ] == ')' )
      {
        ++s;
        if ( level )
          break;
      }
      else
        if ( ( s[ 0 ] == terminator ) || ( s[ 0 ] == extra_term ) )
          break;
    }
  }
  
  ands.prev[ 0 ] = 0;
  nots.prev[ 0 ] = 0;
  ors.prev [ 0 ] = 0;
  
  *outp = o;
  *strp = s;
}

// uses ( ( 6 bytes * ( numb of expressions ) ) + ( ( 6 bytes * ( numb of logical ops ) ) + ( 2 bytes * ( numb of *'s + 1 ) ) )
static S32 parse_wildcard_query( void * out, char const * str, U32 len, char separator, char terminator )
{
  U16 * o = (U16*) out;
  char const * s = str;
  char const * se = str + len;
  char extra_term = separator; // this is a trick. if len == 0xffffffff, then extra_term is set to null
                               //   (to detect end of string), but if we have a length, we set the extra
                               //   terminating character to the normal separator character, which,
                               //   since we check for the extra_term *after* checking for the separator,
                               //   means the extra_term will never trigger (this saves an and clause 
                               //   in the inner loop)
  
  if ( len == 0xffffffff )  // if we don't know the length, set the extra_term to zero to detect end of string
    extra_term = 0;         //   (if they set a specific len, a zero character doesn't terminate)

  internal_parse_wildcard_query( 0, &o, &s, se, extra_term, separator, terminator );

  return (S32) ( ( (char*) o ) - ( (char*) out ) );
}

