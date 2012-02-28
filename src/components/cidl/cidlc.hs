import Text.ParserCombinators.Parsec

data Dir      = To | From | Both -- is data passed to called component, back from it, or both ways?

data CIDLType = Void
              | Addr  -- a void * address
              | USInt -- unsigned, short int
              | SSInt -- short int
              | UInt  -- unsigned int
              | SInt  -- int
              | ULong -- unsigned long 
              | SLong -- long
              | Cbuf String Dir -- cbuffer with length of arg named by string
              | Str String Dir  -- string with length of arg named by string
              | Ptr String Dir  -- buffer with length of arg named by string

data Args     = [CIDLType String]

data FnProto  = FnProto CIDLType String Args

data Includes = [String]

arg = string 
parseFnProto :: GenParser 
