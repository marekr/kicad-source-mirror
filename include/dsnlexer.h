/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2007-2010 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2007 Kicad Developers, see change_log.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _DSNLEXER_H
#define _DSNLEXER_H

#include <cstdio>
#include <string>
#include <boost/ptr_container/ptr_vector.hpp>

#include "fctsys.h"

#include "richio.h"


/**
 * Struct KEYWORD
 * holds a keyword string and its unique integer token.
 */
struct KEYWORD
{
    const char* name;       ///< unique keyword.
    int         token;      ///< a zero based index into an array of KEYWORDs
};

// something like this macro can be used to help initialize a KEYWORD table.
// see SPECCTRA_DB::keywords[] as an example.

//#define TOKDEF(x)    { #x, T_##x }


/**
 * Enum DSN_SYNTAX_T
 * lists all the DSN lexer's tokens that are supported in lexing.  It is up
 * to the parser if it wants also to support them.
 */
enum DSN_SYNTAX_T {
    DSN_NONE = -11,
    DSN_COMMENT = -10,
    DSN_STRING_QUOTE = -9,
    DSN_QUOTE_DEF = -8,
    DSN_DASH = -7,
    DSN_SYMBOL = -6,
    DSN_NUMBER = -5,
    DSN_RIGHT = -4,           // right bracket, ')'
    DSN_LEFT = -3,            // left bracket, '('
    DSN_STRING = -2,          // a quoted string, stripped of the quotes
    DSN_EOF = -1              // special case for end of file
};


/**
 * Class DLEXER
 * implements a lexical analyzer for the SPECCTRA DSN file format.  It
 * reads lexical tokens from the current LINE_READER through the NextTok()
 * function.
 */
class DSNLEXER
{
    char*               next;
    char*               limit;

    typedef boost::ptr_vector<LINE_READER>  READER_STACK;

    READER_STACK        readerStack;            ///< owns all the LINE_READERs by pointer.
    LINE_READER*        reader;                 ///< no ownership. ownership is via readerStack.
    int                 stringDelimiter;
    bool                space_in_quoted_tokens; ///< blank spaces within quoted strings
    bool                commentsAreTokens;      ///< true if should return comments as tokens

    int                 prevTok;        ///< curTok from previous NextTok() call.
    int                 curOffset;      ///< offset within current line of the current token

    int                 curTok;         ///< the current token obtained on last NextTok()
    std::string         curText;        ///< the text of the current token
    std::string         lowercase;      ///< a scratch buf holding token in lowercase

    const KEYWORD*      keywords;
    unsigned            keywordCount;

    /// Use casting char* operator to get start of line, which is dynamic since reader
    /// can be resizing its buffer at each reader->ReadLine() only.
    char*               start() const { return (char*) (*reader); }

    void init();

    int readLine() throw (IOError)
    {
        unsigned len = reader->ReadLine();

        // set next and limit to start() and start() + len.
        // start() is constant until the next ReadLine(), which could resize and
        // relocate reader's line buffer.
        next  = start();
        limit = next + len;

        return len;
    }


    /**
     * Function readLineOrCmt
     * reads a line from the LINE_READER and returns either:
     * <ol>
     * <li> a positive line length (a +1 if empty line)
     * <li> zero of end of file.
     * <li> DSN_COMMENT if the line is a comment
     * </ol>
     */
    int readLineOrCmt();


    /**
     * Function findToken
     * takes a string and looks up the string in the list of expected
     * tokens.
     *
     * @param tok A string holding the token text to lookup, in an
     *   unpredictable case: uppercase or lowercase
     * @return int - DSN_T or -1 if argument string is not a recognized token.
     */
    int findToken( const std::string& tok );

    bool isStringTerminator( char cc )
    {
        if( !space_in_quoted_tokens && cc==' ' )
            return true;

        if( cc == stringDelimiter )
            return true;

        return false;
    }


public:

    /**
     * Constructor DSNLEXER
     * intializes a DSN lexer and prepares to read from aFile which
     * is already open and has aFilename.
     *
     * @param aKeywordTable is an array of KEYWORDS holding \a aKeywordCount.  This
     *  token table need not contain the lexer separators such as '(' ')', etc.
     * @param aKeywordTable is the count of tokens in aKeywordTable.
     */
    DSNLEXER( FILE* aFile, const wxString& aFilename,
            const KEYWORD* aKeywordTable, unsigned aKeywordCount );

    DSNLEXER( const std::string& aClipboardTxt,
        const KEYWORD* aKeywordTable, unsigned aKeywordCount );

    ~DSNLEXER()
    {
    }

    /**
     * Function PushReader
     * manages a stack of LINE_READERs in order to handle nested file inclusion.
     * This function pushes aLineReader onto the top of a stack of LINE_READERs and makes
     * it the current LINE_READER with its own GetSource(), line number and line text.
     * A grammar must be designed such that the "include" token (whatever its various names),
     * and any of its parameters are not followed by anything on that same line,
     * because PopReader always starts reading from a new line upon returning to
     * the original LINE_READER.
     */
    void PushReader( LINE_READER* aLineReader );

    /**
     * Function PopReader
     * deletes the top most LINE_READER from an internal stack of LINE_READERs and
     * in the case of FILE_LINE_READER this means the associated FILE is closed.
     * The most recently used former LINE_READER on the stack becomes the
     * current LINE_READER and its previous position in its input stream and the
     * its latest line number should pertain.  PopReader always starts reading
     * from a new line upon returning to the previous LINE_READER.  A pop is only
     * possible if there are at least 2 LINE_READERs on the stack, since popping
     * the last one is not supported.
     *
     * @return bool - true if there was at least two readers on the stack and
     *  therefore the pop succeeded, else false and the pop failed.
     */
    bool PopReader();

    // Some functions whose return value is best overloaded to return an enum
    // in a derived class.
    //-----<overload return values to tokens>------------------------------

    /**
     * Function NextTok
     * returns the next token found in the input file or DSN_EOF when reaching
     * the end of file.  Users should wrap this function to return an enum
     * to aid in grammar debugging while running under a debugger, but leave
     * this lower level function returning an int (so the enum does not collide
     * with another usage).
     * @return int - the type of token found next.
     * @throw IOError - only if the LINE_READER throws it.
     */
    int NextTok() throw (IOError);

    /**
     * Function NeedSYMBOL
     * calls NextTok() and then verifies that the token read in
     * satisfies bool IsSymbol().
     * If not, an IOError is thrown.
     * @return int - the actual token read in.
     * @throw IOError, if the next token does not satisfy IsSymbol()
     */
    int NeedSYMBOL() throw( IOError );

    /**
     * Function NeedSYMBOLorNUMBER
     * calls NextTok() and then verifies that the token read in
     * satisfies bool IsSymbol() or tok==DSN_NUMBER.
     * If not, an IOError is thrown.
     * @return int - the actual token read in.
     * @throw IOError, if the next token does not satisfy the above test
     */
    int NeedSYMBOLorNUMBER() throw( IOError );

    /**
     * Function CurTok
     * returns whatever NextTok() returned the last time it was called.
     */
    int CurTok()
    {
        return curTok;
    }

    /**
     * Function PrevTok
     * returns whatever NextTok() returned the 2nd to last time it was called.
     */
    int PrevTok()
    {
        return prevTok;
    }

    //-----</overload return values to tokens>-----------------------------


    /**
     * Function SetStringDelimiter
     * changes the string delimiter from the default " to some other character
     * and returns the old value.
     * @param aStringDelimiter The character in lowest 8 bits.
     * @return int - The old delimiter in the lowest 8 bits.
     */
    int SetStringDelimiter( int aStringDelimiter )
    {
        int old = stringDelimiter;
        stringDelimiter = aStringDelimiter;
        return old;
    }

    /**
     * Function SetSpaceInQuotedTokens
     * changes the setting controlling whether a space in a quoted string is
     * a terminator.
     * @param val If true, means
     */
    bool SetSpaceInQuotedTokens( bool val )
    {
        bool old = space_in_quoted_tokens;
        space_in_quoted_tokens = val;
        return old;
    }

    /**
     * Function SetCommentsAreTokens
     * changes the handling of comments.  If set true, comments are returns
     * as single line strings with a terminating newline, else they are
     * consumed by the lexer and not returned.
     */
    bool SetCommentsAreTokens( bool val )
    {
        bool old = commentsAreTokens;
        commentsAreTokens = val;
        return old;
    }

    /**
     * Function IsSymbol
     * tests a token to see if it is a symbol.  This means it cannot be a
     * special delimiter character such as DSN_LEFT, DSN_RIGHT, DSN_QUOTE, etc.  It may
     * however, coincidentally match a keyword and still be a symbol.
     */
    static bool IsSymbol( int aTok );

    /**
     * Function ThrowIOError
     * encapsulates the formatting of an error message which contains the exact
     * location within the input file of something the caller is rejecting.
     */
    void ThrowIOError( wxString aText, int charOffset ) throw (IOError);

    /**
     * Function Expecting
     * throws an IOError exception with an input file specific error message.
     * @param aTok is the token/keyword type which was expected at the current input location.
     * @throw IOError with the location within the input file of the problem.
     */
    void Expecting( int aTok ) throw( IOError );

    /**
     * Function Expecting
     * throws an IOError exception with an input file specific error message.
     * @param aErrorMsg is the token/keyword type which was expected at the
     *         current input location.
     * @throw IOError with the location within the input file of the problem.
     */
    void Expecting( const wxString& aErrorMsg ) throw( IOError );

    /**
     * Function Unexpected
     * throws an IOError exception with an input file specific error message.
     * @param aTok is the token/keyword type which was not expected at the
     *         current input location.
     * @throw IOError with the location within the input file of the problem.
     */
    void Unexpected( int aTok ) throw( IOError );

    /**
     * Function Unexpected
     * throws an IOError exception with an input file specific error message.
     * @param aErrorMsg is the token/keyword type which was not expected at the
     *         current input location.
     * @throw IOError with the location within the input file of the problem.
     */
    void Unexpected( const wxString& aErrorMsg ) throw( IOError );

    /**
     * Function NeedLEFT
     * calls NextTok() and then verifies that the token read in is a DSN_LEFT.
     * If it is not, an IOError is thrown.
     * @throw IOError, if the next token is not a DSN_LEFT
     */
    void NeedLEFT() throw( IOError );

    /**
     * Function NeedRIGHT
     * calls NextTok() and then verifies that the token read in is a DSN_RIGHT.
     * If it is not, an IOError is thrown.
     * @throw IOError, if the next token is not a DSN_RIGHT
     */
    void NeedRIGHT() throw( IOError );

    /**
     * Function GetTokenText
     * returns the C string representation of a DSN_T value.
     */
    const char* GetTokenText( int aTok );

    /**
     * Function GetTokenString
     * returns a quote wrapped wxString representation of a token value.
     */
    wxString GetTokenString( int aTok );

    static const char* Syntax( int aTok );

    /**
     * Function CurText
     * returns a pointer to the current token's text.
     */
    const char* CurText()
    {
        return curText.c_str();
    }

    /**
     * Function CurLineNumber
     * returns the current line number within my LINE_READER
     */
    int CurLineNumber()
    {
        return reader->LineNumber();
    }

    /**
     * Function CurFilename
     * returns the current LINE_READER source.
     * @return const wxString& - the source of the lines of text,
     *   e.g. a filename or "clipboard".
     */
    const wxString& CurSource()
    {
        return reader->GetSource();
    }

    /**
     * Function CurOffset
     * returns the char offset within the current line, using a 1 based index.
     * @return int - a one based index into the current line.
     */
    int CurOffset()
    {
        return curOffset + 1;
    }
};

#endif  // _DSNLEXER_H
