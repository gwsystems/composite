{-# LANGUAGE DeriveDataTypeable #-}

import Data.Graph.Inductive
import Data.Generics
import Data.List

data Component = C String String CArgs COptions -- Component Specification
               | CA Agg                         -- Aggregate
               | CT Component CType deriving (Eq, Show, Data, Typeable) -- Typed Component 

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
          | Dup Component String       -- Duplicate and rename an existing component
          | Comp Component deriving (Eq, Show, Data, Typeable)

-- The CFuse language
data CFuse = CFuse [Stmt] deriving (Eq, Show, Data, Typeable)

-- Component construction
comp :: String -> String -> String -> Component
comp i n s = C i n "" COpt {schedParams = s, isSched = False, isFaultHndl = False, isInit = False}

agg :: [Component] -> Component
agg cs = CA cs -- map cunwrap cs 

-- wrapped comp
wcomp  :: String -> String -> String -> Stmt
wcomp i n s     = Comp (comp i n s)
uwcomp :: Stmt -> Component
uwcomp (Comp c) = c

-- set component fields
cSetSched :: Component -> String -> Component
cSetSched (C a b c o) s = C a b c (o {schedParams=s})
cSetArgs  (C a b _ c) s = C a b s c
cIsSched  :: Component -> Component
cIsSched  (C a b c o)   = C a b c (o {isSched=True})
cIsFltH   (C a b c o)   = C a b c (o {isFaultHndl=True})
cIsInit   (C a b c o)   = C a b c (o {isInit=True})

-- Dependencies
dep  c1 c2   = Dep c1 c2
depT c1 c2 s = DepTSyn c1 c2 s

progConcat :: CFuse -> CFuse -> CFuse
progConcat (CFuse p1) (CFuse p2) = (CFuse (p1 ++ p2))

-- Querying the structure
lstCs :: CFuse -> [Component]
lstCs p = let f = (\c -> case c of 
                           (C _ _ _ _) -> True
                           _ -> False)
          in nub $ listify f p

lstAggs :: CFuse -> [Component]
lstAggs p = let f = (\c -> case c of
                             (CA _) -> True 
                             _ -> False)
            in nub $ listify f p

lstDeps :: CFuse -> [Stmt]
lstDeps p = let f = (\c -> case c of
                             (Dep _ _) -> True 
                             _ -> False)
            in nub $ listify f p

-- Take Cs and produce CTs
-- This will need to be very complicated and read all the type info from the FS
annotateCs :: [Component] -> [Component]
annotateCs cs = map f cs
    where f c = case c of 
                  (C a b c d) -> (CT (C a b c d) (T [""] [("","")]))

compLift cs = map (\a -> case a of 
                           (CT a' _) -> a') cs

compIndex :: [Stmt] -> Component -> Int
compIndex cts c = n
    where cs = compLift cts
          a = elemIndex c cs
          n = case a of 
                (Just a') -> a'
                (Nothing) -> -1

-- Should only pass in CTs
cmkVertices :: [Component] -> [LNode]
cmkVertices cs = map f (compLift cs)
    where f c = ((compIndex cs c), c)

-- Should only pass in Deps and (compIndex cs)
cmkEdges :: [Stmt] -> (Component -> Int) -> [LEdge]
cmkEdges ds idxf = map f ds
    where f (Dep c1 c2) = (((idxf c1), (idxf c2)), "")

compGraph :: CFuse -> Graph a b
compGraph p = let cts = annotateCs $ lstCs p
                  vs = cmkVertices cts
                  es = cmkEdges (lstDeps p) (compIndex cts)
              in mkGraph vs es
              

main :: IO ()
main = let b = comp "no_interface" "booter" ""
           c = comp "something" "comp" ""
           d = comp "no_interface" "booter" ""
           e = annotateCs $ lstCs $ CFuse 
              [ Comp b
              , Comp (agg [cSetSched (comp "a" "b" "") "a1", comp "c" "d" "", d])
              , dep b c
              , dep c d]
           i = compIndex e
       in print $ (show $ i b) ++ (show $ i c) ++ (show $ i d)

