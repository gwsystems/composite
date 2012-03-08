{-# LANGUAGE DeriveDataTypeable, NoMonomorphismRestriction #-}

{- 
TODO:
- check for cycles
- output aggregates as clusters correctly -- requires enabling dependencies to and from clusters (in dot)
- generate dependencies between components when there are dependencies to and from aggregates
- runscript backend
-}

import Data.Graph.Inductive
import Data.Generics
import Data.List
import Data.IORef
import Data.GraphViz
import System(getArgs)
import System.Environment
import System.IO
import System.IO.Unsafe
import Data.Text.Lazy(unpack)

data Component = CS CName CArgs COptions -- Component Specification
               | CA CName [Component]    -- Aggregate
               | CR Component [IFName]   -- Restricted interface
               | CD Component String deriving (Eq, Show, Data, Typeable)
                                         -- Duplicate and rename a component

-- The name has an interface, implementation, and possibly a duplicated name
data CName    = CName String String deriving (Eq, Show, Data, Typeable)

data COptions = COpt { schedParams :: String
                     , isSched :: Bool
                     , isFaultHndl :: Bool
                     , isInit :: Bool
                     } deriving (Eq, Show, Data, Typeable)

type CArgs  = String -- Initial arguments to component

-- Type is two lists: dependents + interface functions
data CType  = T [Fn] [Fn] deriving (Eq, Show, Data, Typeable)
type Fn     = String  -- function name
type IFName = String -- interface name

-- Dependency from the first to the second
data Dep    = Dep Component Component deriving (Eq, Show, Data, Typeable) 

data Stmt   = SDep  Dep
            | SComp Component deriving (Eq, Show, Data, Typeable)

-- set component fields
cSetSchedP :: Component -> String -> Component
cSetSchedP (CS n c o) s = CS n c (o {schedParams=s})

cSetArgs   (CS n _ c) s = CS n s c

cIsSched  :: Component -> Component
cIsSched  (CS n c o)   = CS n c (o {isSched=True})
cIsFltH   (CS n c o)   = CS n c (o {isFaultHndl=True})
cIsInit   (CS n c o)   = CS n c (o {isInit=True})


-- Type Data-base with a function for mapping from a component name to
-- component type, and for mapping from an interface name to its
-- exported functions.
data Tdb    = Tdb CFuse (CName -> CType) (IFName -> [Fn]) 
findIFTc :: [(IFName, [Fn])] -> IFName -> [Fn]
findIFTc [] i            = ["Internal Cfuse interface typing error."]
findIFTc ((r,ss):left) i = if r == i then ss 
                           else findIFTc left i

findCompTc :: [(CName, CType)] -> CName -> CType
findCompTc [] c               = T ["Internal Cfuse component typing error."] []
findCompTc ((cname, ct):ss) c = if cname == c then ct
                                else findCompTc ss c

constructTdb :: CFuse -> [(CName, CType)] -> [(IFName, [Fn])] -> Tdb
-- use currying here to construct the functions
constructTdb p cm im = Tdb p (findCompTc cm) (findIFTc im)

-- Use these functions to query the type data-base
queryIFT :: Tdb -> IFName -> [Fn]
queryIFT t n = case t of Tdb p f g -> g n

queryCT  :: Tdb -> CName -> CType
queryCT t n  = case t of Tdb p f g -> f n

compName :: Component -> CName
compName (CS n _ _) = n
compName (CR c _)   = compName c
compName (CD c _)   = compName c
compName (CA n _)   = n

cexps :: Tdb -> Component -> [Fn]
cexps t c@(CS n _ _) = cexpsC t c
cexps t   (CR c' is) = intersect (foldr (\i  r -> r ++ (queryIFT t i)) [] is) (cexps t c')
cexps t   (CD c' _)  = cexps t c'
cexps t   (CA _ cs)  = foldr (\c' r -> r ++ (cexps t c')) [] cs
cexpsC t c = case (queryCT t $ compName c) of T es ds -> es

cdeps :: Tdb -> Component -> [Fn]
cdeps t c@(CS n _ _) = cdepsC t c
cdeps t   (CR c' _)  = cdeps t c'
cdeps t   (CD c' _)  = cdeps t c'
cdeps t   (CA _ cs)  = foldr (\c' r -> r ++ (depsUnsatisfied t p c')) [] cs
    where p = case t of (Tdb p _ _) -> p
cdepsC t c = case (queryCT t $ compName c) of T es ds -> ds

--[ Checking for undefined dependencies ]--

depList :: Tdb -> Component -> Dep -> [Fn]
depList t c d = case d of (Dep a b) -> if a == c then cexps t b else []

depsUnsatisfied :: Tdb -> CFuse -> Component -> [Fn]
depsUnsatisfied t f c = (cdeps t c) \\ exps
    where exps = concat $ map (depList t c) (lstDeps f)

undefinedDependencies :: Tdb -> CFuse -> [(String, [Fn])]
undefinedDependencies t p = 
    let cs    = lstCs p
        ns    = map printCName cs
        ds    = map (depsUnsatisfied t p) cs
        z     = zip ns ds
        undef = filter (\(n, fns) -> not (fns == [])) z
    in undef

stringifyUndefDeps :: Tdb -> CFuse -> String
stringifyUndefDeps t p =
    let
        pfn = (\(n, fns) s -> s ++ "Error: component " ++ n 
                              ++ " has undefined dependencies:\n\t" ++ (show fns) ++ "\n")
    in foldr pfn "" (undefinedDependencies t p)

generateAggDeps :: Tdb -> CFuse -> [Dep]
generateAggDeps t p = 

--[ Checking for aggregate independence (i.e. only dependencies within or outside of an aggregate) ]--

-- outbound links?
aggOut (CA (CName _ n) cs) ds = foldr chk "" ds
    where chk (Dep a b) s = if ((elem a cs) && (not (elem b cs))) 
                            then s ++ "Error: component " ++ (printCName a) ++ " in aggregate "
                                     ++ n ++ " depends on " ++ (printCName b) 
                                     ++ " outside of the aggregate\n\tSuggestion: just make " ++ n 
                                     ++ " depend on " ++ (printCName b) ++ "\n"
                            else s

-- inbound links?
aggIn (CA (CName _ n) cs) ds = foldr chk "" ds
    where chk (Dep a b) s = if ((elem b cs) && (not (elem a cs))) 
                            then s ++ "Error: component " ++ (printCName a) ++ " not in aggregate " 
                                     ++ n ++ " depends on " ++ (printCName b) 
                                     ++ " inside the aggregate\n\tSuggestion: just depend on " ++ n ++ "\n"
                            else s


-- aggregates, dependencies, and the resulting errors
aggsErrors :: [Component] -> [Dep] -> String
aggsErrors as ds = foldr aggErrs "" as
    where aggErrs a s = s ++ (aggOut a ds) ++ (aggIn a ds)

-- Highest-level node
data CFuse = CFuse [Stmt] deriving (Eq, Show, Data, Typeable)

--[ System construction utility functions ]--
ca :: String -> String -> String -> String -> Component
ca i n s a = CS (CName i n) a COpt {schedParams = s, isSched = False, isFaultHndl = False, isInit = False}

c :: String -> String -> Component
c i n = CS (CName i n) [] COpt {schedParams = [], isSched = False, isFaultHndl = False, isInit = False}

agg :: String -> [Component] -> Component
agg name cs = CA (CName "aggregate" name) cs

aggTop = agg "all" []

-- First argument is the list of aggregate components.  Yields the
-- path of aggregates this component is in.
aggPath :: [Component] -> Component -> [Component]
aggPath as c = let
    aggIn c' = filter (\a@(CA _ cs) -> elem c' cs) as
    aggC  c' = let a = aggIn c'
               in if (a == []) then aggTop else (a !! 0)
    aggP  c' = let a = (aggC c') 
               in if a == aggTop then [a] else ((aggP a) ++ [a])
    in aggP c

aggPathS :: [Component] -> Component -> String
aggPathS as c = concat (map str as)
    where 
      p     = aggPath as c
      str c = case c of (CA (CName _ n) _) -> n 

-- How do we find a system-wide free variable?  This is the easiest
-- way, without threading data everywhere, in fact a technique the ghc
-- uses itself.  I'm, of course, going to hell for using
-- unsafePerformIO, but everything's a trade-off.
counter = unsafePerformIO $ newIORef 0
newDname _ = unsafePerformIO $
             do
               i <- readIORef counter
               writeIORef counter (i+1)
               return $ show i

-- OK, the idea here is that we want a _new_ version of the component.
-- Thus, we generate a fresh name for it.  This works because the Eq
-- class that Component derives will make this duplicate non-equal to
-- other verions of the component.
dup :: Component -> Component
dup c = CD c (newDname c)

restrict :: Component -> [String] -> Component
restrict c is = CR c is

-- Dependencies
dep  c1 c2   = Dep c1 c2

progConcat :: CFuse -> CFuse -> CFuse
progConcat (CFuse p1) (CFuse p2) = CFuse (p1 ++ p2)

-- Querying the structure
lstCs :: CFuse -> [Component]
lstCs p = let f = (\c -> case c of 
                           (CS _ _ _) -> True
                           (CD _ _)   -> True
                           _          -> False)
          in nub $ listify f p

lstOnlyCs :: CFuse -> [Component]
lstOnlyCs p = let f = (\c -> case c of 
                           (CS _ _ _) -> True
                           _          -> False)
          in nub $ listify f p

lstAggs :: CFuse -> [Component]
lstAggs p = let f = (\c -> case c of
                             (CA _ _) -> True 
                             _        -> False)
            in nub $ listify f p

lstDeps :: CFuse -> [Dep]
lstDeps p = let f = (\c -> case c of (Dep _ _) -> True)
            in nub $ listify f p

lstIFs :: CFuse -> [Component]
lstIFs p = let f = (\c -> case c of
                            (CR _ _) -> True
                            _        -> False)
           in nub $ listify f p


--[ Graph Backend ]--

compIndex :: [Component] -> Component -> Int
compIndex cts c = n
    where cs = cts
          a  = elemIndex c cs
          n  = case a of 
                 (Just a') -> a'
                 (Nothing) -> -1

-- Should only pass in CTs
cmkVertices :: [Component] -> [LNode Component]
cmkVertices cs = map f (cs)
    where f c = ((compIndex cs c), c)

-- Should only pass in Deps and (compIndex cs)
cmkEdges :: [Dep] -> (Component -> Int) -> [LEdge String]
cmkEdges ds idxf = map f ds
    where f (Dep c1 c2)       = ((idxf c1), (idxf c2), "")

--type FusedSys (DynGraph Component String, [Stmt], [Component])

compGraph :: CFuse -> Gr Component String
compGraph p = let cts = lstCs p
                  vs  = cmkVertices cts
                  es  = cmkEdges (lstDeps p) (compIndex cts)
              in mkGraph vs es

--cParams = GraphvizParams n nl el cl nl
cParams aggfn = defaultParams { clusterBy = cb
                              , fmtNode   = fn
                              , fmtEdge   = fe}
    where 
      cb (i, c)                             = C (aggfn c) (N (i, c))
      fn (_, (CS (CName s1 s2) _ _))        = [toLabel (s1 ++ "." ++ s2)]
      fn (_, (CD (CS (CName s1 s2) _ _) _)) = [toLabel (s1 ++ "." ++ s2)]
      fn _                                  = [toLabel ""]
      fe (_, _, e)                          = [toLabel e]

translateToDot :: CFuse -> String
translateToDot p =  unpack (printDotGraph $ graphToDot (cParams (aggPathS (lstAggs p))) (compGraph p))

-- dependency list construction
depl :: [(Component, [Component])] -> [Stmt]
depl dl = concat $ map (\(c, cs) -> map (\d -> SDep $ dep c d) cs) dl

cnames :: [Component] -> [CName]
cnames cs = map (\c -> nameC c) cs

nameC :: Component -> CName
nameC (CS n _ _) = n
nameC _          = CName "" ""

name2str :: String -> CName -> String
name2str s (CName a b) = a ++ s ++ b

printCName :: Component -> String
printCName = (name2str ".") . nameC

ifns :: CFuse -> [IFName]
ifns p = concat $ map ifn (lstIFs p)
    where ifn (CR _ ns) = ns

implDir = "../implementation/"
expIfs  = "/.exported_interfaces"
fnDeps  = "/.fn_dependencies"
ifDir   = "../interface/"
ifExps  = "/.fn_exports"

perror = hPutStrLn stderr

-- Get the type information from the components...
readCTypes :: CName -> IO (CName, CType)
readCTypes n = do
  e <- (readFile $ implDir ++ (name2str "/" n) ++ expIfs) `catch` 
       (\e -> do {perror ("Error: no component named " ++ (name2str "." n)); return "";})
  d <- (readFile $ implDir ++ (name2str "/" n) ++ fnDeps) `catch` 
       (\e -> do {perror ("Error: no component named " ++ (name2str "." n)); return "";})
  return (n, T (lines e) (lines d))
readAllCTypes ns = mapM readCTypes ns

-- ...and the interfaces.
readIFfns :: IFName -> IO (IFName, [String])
readIFfns i = do
  e <- (readFile $ ifDir ++ i ++ ifExps) `catch` 
       (\e -> do {perror ("Error: no interface named " ++ i); return "";})
  return (i, (lines e))
readAllIFfns :: [IFName] -> IO [(IFName, [String])]
readAllIFfns is = mapM readIFfns is

cfuse ss = CFuse $ concat ss

program = system -- sysAgg

main :: IO ()
main = let p   = cfuse program
           ifs = ifns p
       in do 
--         args  <- getArgs
         ifnfo <- readAllIFfns ifs
         cnfo  <- readAllCTypes (cnames (lstOnlyCs p))
         let tdb = constructTdb p cnfo ifnfo
             err = (stringifyUndefDeps tdb p) ++ (aggsErrors (lstAggs p) (lstDeps p))
         if err == "" then (putStrLn (translateToDot p)) else (hPutStrLn stderr err)

-- unit_cbuf.sh
system = let c0     = c "no_interface" "comp0"
             fprr   = c "sched" "fprr"
             mm     = c "mem_mgr" "naive"
             print  = c "printc" "linux_log"
             schedconf = c "sched_conf" "http_config"
             st     = c "stack_trace" "same_pd"
             bc     = c "sched" "base_case"
             cg     = c "cgraph" "static"
--             boot   = restrict (c "no_interface" "boot") ["cinfo"]
             boot   = c "no_interface" "boot"
             mpool  = c "mem_pool" "naive"
             sm     = c "stkmgr" "naive"
             l      = c "lock" "two_phase"
             e      = c "evt" "edge"
             te     = c "timed_blk" "timed_evt"
             stat   = c "no_interface" "stat"
             buf    = c "cbuf_c" "naive"
             tp     = c "no_interface" "tmem_policy"
             va     = c "valloc" "simple"
             ucbuf1 = c "tests" "unit_cbuf1"
             ucbuf2 = c "tests" "unit_cbuf2"
             ucbuf3 = dup ucbuf1
         in [depl [ (c0, [fprr])
                  , (fprr, [print, mm, st, schedconf, bc])
                  , (l, [fprr, mm, print])
                  , (te, [sm, print, fprr, mm, va])
                  , (mm, [print])
                  , (e, [sm, fprr, print, mm, l, va])
                  , (stat, [sm, te, fprr, l, print, e])
                  , (st, [print])
                  , (schedconf, [print])
                  , (bc, [print])
                  , (boot, [print, fprr, mm, cg])
                  , (sm, [print, fprr, mm, boot, va, l, mpool])
                  , (buf, [boot, sm, fprr, print, l, mm, va, mpool])
                  , (mpool, [print, fprr, mm, boot, va, l])
                  , (tp, [sm, buf, print, te, fprr, schedconf, mm, va, mpool])
                  , (va, [fprr, print, mm, l, boot])
                  , (ucbuf1, [fprr, sm, ucbuf2, print, mm, va, buf, l])
                  , (ucbuf2, [sm, print, mm, va, buf, l])
                  , (ucbuf3, [fprr, sm, ucbuf2, print, mm, va, buf, l])
                  , (cg, [fprr])]]

sysAgg = let c0     = c "no_interface" "comp0"
             fprr   = c "sched" "fprr"
             mm     = c "mem_mgr" "naive"
             print  = c "printc" "linux_log"
             schedconf = c "sched_conf" "http_config"
             st     = c "stack_trace" "same_pd"
             bc     = c "sched" "base_case"
             cg     = c "cgraph" "static"
             ll     = agg "ll" [c0, fprr, mm, print, schedconf, st, bc, cg]
             boot   = c "no_interface" "boot"
             mpool  = c "mem_pool" "naive"
             sm     = c "stkmgr" "naive"
             l      = c "lock" "two_phase"
             e      = c "evt" "edge"
             te     = c "timed_blk" "timed_evt"
             stat   = c "no_interface" "stat"
             buf    = c "cbuf_c" "naive"
             tp     = c "no_interface" "tmem_policy"
             va     = c "valloc" "simple"
             ucbuf1 = c "tests" "unit_cbuf1"
             ucbuf2 = c "tests" "unit_cbuf2"
         in [depl [ (c0, [fprr])
                  , (fprr, [print, mm, st, schedconf, bc])
                  , (bc, [print])
                  , (mm, [print])
                  , (cg, [fprr])
                  , (st, [print])
                  , (schedconf, [print])

                  , (l, [ll])
                  , (te, [sm, ll, va])
                  , (e, [sm, ll, l, va])
                  , (stat, [sm, te, ll, l, e])
                  , (boot, [ll])
                  , (sm, [ll, boot, va, l, mpool])
                  , (buf, [boot, sm, l, ll, va, mpool])
                  , (mpool, [ll, boot, va, l])
                  , (tp, [sm, buf, te, ll, va, mpool])
                  , (va, [ll, l, boot])
                  , (ucbuf1, [ll, sm, ucbuf2, va, buf, l])
                  , (ucbuf2, [sm, ll, va, buf, l])]]
