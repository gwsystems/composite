{-# LANGUAGE DeriveDataTypeable, NoMonomorphismRestriction #-}

import Data.Graph.Inductive
import Data.Generics
import Data.List
import Data.GraphViz

data Component = CS String String CArgs COptions -- Component Specification
               | CA Agg                          -- Aggregate
               | CT Component CType              -- Typed Component
               | Dup Component String  deriving (Eq, Show, Data, Typeable) -- Duplicate and rename an existing component

data COptions = COpt { schedParams :: String
                     , isSched :: Bool
                     , isFaultHndl :: Bool
                     , isInit :: Bool
                     } deriving (Eq, Show, Data, Typeable)

type CArgs = String -- Initial arguments to component
-- List of components in the aggregate
type Agg = [Component]

-- Type is two lists: dependents + interface functions
data CType = T [Fn] [(IF, Fn)] deriving (Eq, Show, Data, Typeable)
type Fn = String -- function name
type IF = String -- interface

data Stmt = As CType Component         -- Stype must be a subtype (wrt structural subtyping) of Component's type
          | Dep Component Component    -- Dependency from the first to the second
          | DepTSyn Component Component String -- Dependency with local type synonym
          | Comp Component deriving (Eq, Show, Data, Typeable)

-- The CFuse language
data CFuse = CFuse [Stmt] deriving (Eq, Show, Data, Typeable)

-- Component construction
comp :: String -> String -> String -> String -> Component
comp i n s a = CS i n a COpt {schedParams = s, isSched = False, isFaultHndl = False, isInit = False}

agg :: [Component] -> Component
agg cs = CA cs -- map cunwrap cs 

-- wrapped comp
wcomp  :: String -> String -> String -> String -> Stmt
wcomp i n s a   = Comp (comp i n s a)
uwcomp :: Stmt -> Component
uwcomp (Comp c) = c

-- set component fields
cSetSched :: Component -> String -> Component
cSetSched (CS a b c o) s = CS a b c (o {schedParams=s})
cSetArgs  (CS a b _ c) s = CS a b s c
cIsSched  :: Component -> Component
cIsSched  (CS a b c o)   = CS a b c (o {isSched=True})
cIsFltH   (CS a b c o)   = CS a b c (o {isFaultHndl=True})
cIsInit   (CS a b c o)   = CS a b c (o {isInit=True})

-- Dependencies
dep  c1 c2   = Dep c1 c2
depT c1 c2 s = DepTSyn c1 c2 s

progConcat :: CFuse -> CFuse -> CFuse
progConcat (CFuse p1) (CFuse p2) = (CFuse (p1 ++ p2))

-- Querying the structure
lstCs :: CFuse -> [Component]
lstCs p = let f = (\c -> case c of 
                           (CS _ _ _ _)   -> True
                           (Dup _ _)      -> True
                           _ -> False)
          in nub $ listify f p

lstAggs :: CFuse -> [Component]
lstAggs p = let f = (\c -> case c of
                             (CA _) -> True 
                             _      -> False)
            in nub $ listify f p

lstDeps :: CFuse -> [Stmt]
lstDeps p = let f = (\c -> case c of
                             (Dep _ _)       -> True 
                             (DepTSyn _ _ _) -> True 
                             _ -> False)
            in nub $ listify f p

-- Take Cs and produce CTs
-- This will need to be very complicated and read all the type info from the FS
annotateCs :: [Component] -> [Component]
annotateCs cs = map f cs
    where f (CS i n c d)          = (CT (CS i n c d) (T [""] [("","")]))
          f (CA cs)               = (CT (CA cs) (T [""] [("","")]))
          f (Dup (CS i n c d) n') = (CT (CS i n' c d) (T [""] [("","")]))

compLift cs = map (\a -> case a of 
                           (CT a' _) -> a') cs

compIndex :: [Component] -> Component -> Int
compIndex cts c = n
    where cs = compLift cts
          a  = elemIndex c cs
          n  = case a of 
                 (Just a') -> a'
                 (Nothing) -> -1

-- Should only pass in CTs
cmkVertices :: [Component] -> [LNode Component]
cmkVertices cs = map f (compLift cs)
    where f c = ((compIndex cs c), c)

-- Should only pass in Deps and (compIndex cs)
cmkEdges :: [Stmt] -> (Component -> Int) -> [LEdge String]
cmkEdges ds idxf = map f ds
    where f (Dep c1 c2)       = ((idxf c1), (idxf c2), "")
          f (DepTSyn c1 c2 s) = ((idxf c1), (idxf c2), s)

--type FusedSys (DynGraph Component String, [Stmt], [Component])

compGraph :: CFuse -> Gr Component String
compGraph p = let cts = annotateCs $ lstCs p
                  vs  = cmkVertices cts
                  es  = cmkEdges (lstDeps p) (compIndex cts)
              in mkGraph vs es

--cParams = GraphvizParams n n1 el () nl
cParams = nonClusteredParams { globalAttributes = []
                             , fmtNode = fn
                             , fmtEdge = fe}
          where 
            fn (_, (CT (CS s1 s2 _ _) _)) = [toLabel (s1 ++ "." ++ s2)]
            fn (_, (CS s1 s2 _ _))        = [toLabel (s1 ++ "." ++ s2)]
            fn _                          = [toLabel ""]
            fe (_, _, e)                  = [toLabel e]

translateToDot p =  printDotGraph $ graphToDot cParams (compGraph p)

-- dependency list
depl :: [(Component, [Component])] -> [Stmt]
depl dl = concat $ map (\(c, cs) -> map (\d -> dep c d) cs) dl

-- component list
compl :: [Component] -> [Stmt]
compl cs = map (\c -> Comp c) cs

cfuse ss = CFuse $ concat ss

main :: IO ()
main = let a = comp  "a" "b" "a1" ""
           b = comp "no_interface" "booter" "" ""
           c = comp "something" "comp" "" ""
           d = comp "no_interface" "other" "" ""
           e = comp "c" "d" "" ""
           f = agg [a, e, d]
           p = cfuse [depl [ (b, [c, d])
                           , (a, [b, d])
                           , (c, [a, d, e])]]
       in print $ translateToDot p
