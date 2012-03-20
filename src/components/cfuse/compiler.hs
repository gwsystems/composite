{-# LANGUAGE DeriveDataTypeable, NoMonomorphismRestriction #-}

{- 
TODO:
- check for cycles
- output aggregates as clusters correctly -- requires enabling dependencies to and from clusters (in dot)
- runscript backend
- check for dependencies that resolve to an ambiguous exporter
- check that two componenents in the same aggregate don't define
  overlapping functions when there is a dependency on the aggregate
- better error reporting: within an aggregate, only report those that are undefined at the aggregate level.
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

data Component = CS CName CArgs COptions Uniq (Maybe Component)
               -- Component Specification: name, initial arguments, options, a unique identifier, original component (i.e. duplicates point to the original)
               | CA CName [Component] Uniq (Maybe Component)
               -- Aggregate: Name, Components in aggregate, and a pointer to the original (see above)
               | CR Component [IFName] deriving (Eq, Show, Data, Typeable)
               -- Restricted interface: component to be restricted, and the list of interfaces restricted to

-- The name has an interface, implementation, and possibly a duplicated name
data CName    = CName String String deriving (Eq, Show, Data, Typeable)

data COptions = COpt { schedParams :: String
                     , isSched :: Bool
                     , isInit :: Bool
                     } deriving (Eq, Show, Data, Typeable)

type CArgs  = String -- Initial arguments to component
type Uniq   = String

-- Type is two lists: dependents + interface functions
data CType  = T [Fn] [Fn] deriving (Eq, Show, Data, Typeable)
type Fn     = String  -- function name
type IFName = String -- interface name

-- Dependency from the first to the second
data Dep    = Dep Component Component deriving (Eq, Show, Data, Typeable) 

data Stmt   = SDep  Dep
            | SComp Component deriving (Eq, Show, Data, Typeable)

--[ System construction utility functions ]--
ca :: String -> String -> String -> String -> Component
ca i n s a = CS (CName i n) a COpt {schedParams = s, isSched = False, isInit = False} (newUniqName 1) Nothing

c :: String -> String -> Component
c i n = CS (CName i n) [] COpt {schedParams = [], isSched = False, isInit = False} (newUniqName 1) Nothing

-- set component fields
cSetSchedP :: Component -> String -> Component
cSetSchedP (CS n c o u orig) s = CS n c (o {schedParams=s}) u orig

cSetArgs   (CS n _ c u orig) s = CS n s c u orig

cSetSched  :: Component -> Component
cSetSched  (CS n c o u orig)   = CS n c (o {isSched=True}) u orig
cSetInit   (CS n c o u orig)   = CS n c (o {isInit=True}) u orig

agg :: String -> [Component] -> Component
agg name cs = CA (CName "aggregate" name) cs (newUniqName 1) Nothing

-- OK, the idea here is that we want a _new_ version of the component.
-- Thus, we generate a fresh name for it.  This works because the Eq
-- class that Component derives will make this duplicate non-equal to
-- other verions of the component.
dup :: Component -> Component
dup c@(CS n i o u orig) = CS n i COpt {schedParams = schedParams o, isSched = isSched o, isInit = isInit o} 
                          (newUniqName 1) (Just (case orig of
                                                   Just c' -> c'
                                                   Nothing -> c))
dup a@(CA n cs u orig)  = CA n (map dup cs) (newUniqName 1) (Just (case orig of 
                                                                     (Just c') -> c'
                                                                     (Nothing) -> a))

restrict :: Component -> [String] -> Component
restrict c is = CR c is

dep  c1 c2   = Dep c1 c2

--[ End system constructor utility functions ]-- 


cIsSched :: Component -> Bool
cIsSched   (CS n a o u u') = isSched o
cIsInit    (CS n a o u u') = isInit o
cGetSchedP (CS n a o u u') = schedParams o

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
compName (CS n _ _ _ _) = n
compName (CR c _)       = compName c
compName (CA n _ _ _)   = n

cexps :: Tdb -> Component -> [Fn]
cexps t c@(CS n _ _ _ _) = cexpsC t c
cexps t   (CR c' is)     = intersect (concatMap (\i -> queryIFT t i) is) (cexps t c')
cexps t   (CA _ cs _ _ ) = nub $ concatMap (\c' -> cexps t c') cs
cexpsC t c = case (queryCT t $ compName c) of T es ds -> es

cdeps :: Tdb -> [Dep] -> Component -> [Fn]
cdeps t ds c@(CS n _ _ _ _) = cdepsC t c
cdeps t ds   (CR c' _)      = cdeps t ds c'
cdeps t ds   (CA _ cs _ _)  = nub $ concatMap (\c' -> depsUnsatisfied t p ds c') cs
    where p = case t of (Tdb p _ _) -> p
cdepsC t c = case (queryCT t $ compName c) of T es ds -> ds

--[ Checking for undefined dependencies ]--

-- the exported functions of all depended-on components
depExpList :: Tdb -> Component -> Dep -> [Fn]
depExpList t c d = case d of (Dep a b) -> if a == c then cexps t b else []

depsUnsatisfied :: Tdb -> CFuse -> [Dep] -> Component -> [Fn]
depsUnsatisfied t p ds c = (cdeps t ds c) \\ exps
    where exps = concatMap (depExpList t c) ds

undefinedDependencies :: Tdb -> CFuse -> [(String, [Fn])]
undefinedDependencies t p = 
    let cs    = acs ++ as
        acs   = lstCs p
        ds    = lstDeps p
        as    = lstAggs p
        ns    = map printCName cs
        uds   = map (depsUnsatisfied t p ds) cs
        z     = zip ns uds
        undef = filter (\(n, fns) -> not (fns == [])) z
        topA  = (aggPath as (acs !! 0)) !! 0
        isErr = not $ depsUnsatisfied t p ds topA == []
    in if (isErr) then undef else []

stringifyUndefDeps :: Tdb -> CFuse -> String
stringifyUndefDeps t p =
    let
        pfn = (\(n, fns) s -> s ++ "Error: component " ++ n 
                              ++ " has undefined dependencies:\n\t" ++ (show fns) ++ "\n")
    in foldr pfn "" (undefinedDependencies t p)

inDeps :: Tdb -> [Dep] -> Component -> [Component] -> [Dep]
inDeps t ds a cs = 
    let
        doesExp c a = not ((intersect (cexps t a) (cdeps t ds c)) == [])
        needDep     = filter (\c -> doesExp c a) cs 
    in map (\c -> Dep c a) needDep

outDeps :: Tdb -> [Dep] -> Component -> [Component] -> [Dep]
outDeps t ds a cs = 
    let
        doesExp c a = not ((intersect (cexps t c) (cdeps t ds a)) == [])
        needDep     = filter (\c -> (doesExp c a)) cs 
    in map (\c -> Dep a c) needDep

depsWith cs deps = concatMap (\c -> filter (\(Dep f t) -> t == c || f == c) deps) cs

generateAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateAggDeps t deps a@(CA n cs _ _) = all where
    -- comp dependencies from agg
    lout       = map (\(Dep f t) -> t) $ filter (\(Dep f t) -> f == a) deps 
    newOutDeps = concatMap (\cina -> outDeps t deps cina lout) cs
    -- comp dependencies coming into agg
    lin        = map (\(Dep f t) -> f) $ filter (\(Dep f t) -> t == a) deps
    newInDeps  = concatMap (\cina -> inDeps t deps cina lin) cs
    all        = newInDeps ++ newOutDeps
generateAggDeps t deps a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

generateOutAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateOutAggDeps t deps a@(CA n cs _ _) = newOutDeps where
    -- comp dependencies from agg
    lout       = map (\(Dep f t) -> t) $ filter (\(Dep f t) -> f == a) deps 
    newOutDeps = concatMap (\cina -> outDeps t deps cina lout) cs
generateOutAggDeps t deps a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

generateInAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateInAggDeps t deps a@(CA n cs _ _) = newInDeps where
    lin        = map (\(Dep f t) -> f) $ filter (\(Dep f t) -> t == a) deps
    newInDeps  = concatMap (\cina -> inDeps t deps cina lin) cs
generateInAggDeps t deps  a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

-- the components should be a list of aggregates
generateAllAggDepsOut :: Tdb -> [Dep] -> [Component] -> [Dep]
generateAllAggDepsOut t ds []     = []
generateAllAggDepsOut t ds (c:cs) = (generateOutAggDeps t ds c) ++ (generateAllAggDepsOut t ds cs)

generateAllAggDepsIn :: Tdb -> [Dep] -> [Component] -> [Dep]
generateAllAggDepsIn t ds []     = []
generateAllAggDepsIn t ds (c:cs) = (generateInAggDeps t ds c) ++ (generateAllAggDepsIn t ds cs)

generateAllAggDeps :: Tdb -> [Dep] -> [Component] -> [Dep]
generateAllAggDeps t ds as = dout ++ din
    where dout = generateAllAggDepsOut t ds as
          din  = generateAllAggDepsIn  t (ds ++ dout) as

-- a component should be in zero or one aggregate
aggSingularS :: [Component] -> [Component] -> String
aggSingularS as cs = let
    aggs c        = filter (\a@(CA _ cs _ _) -> elem c cs) as
    aggsS c       = if ((length (aggs c)) > 1) then 
                        ("Error: component " ++ (printCName c) ++ 
                         " is part of multiple aggregates: " ++ 
                         (foldr (\a s -> (printCName a) ++ ", " ++ s) "" $ aggs c))
                    else ""
    accum []      = ""
    accum (c:cs)  = (aggsS c) ++ accum cs
    in accum cs

--[ Checking for aggregate independence (i.e. only dependencies within or outside of an aggregate) ]--

-- outbound links?
aggOut (CA (CName _ n) cs _ _) ds = foldr chk "" ds
    where chk (Dep a b) s = if ((elem a cs) && (not (elem b cs))) 
                            then s ++ "Error: component " ++ (printCName a) ++ " in aggregate "
                                     ++ n ++ " depends on " ++ (printCName b) 
                                     ++ " outside of the aggregate\n\tSuggestion: just make " ++ n 
                                     ++ " depend on " ++ (printCName b) ++ "\n"
                            else s

-- inbound links?
aggIn (CA (CName _ n) cs _ _) ds = foldr chk "" ds
    where chk (Dep a b) s = if ((elem b cs) && (not (elem a cs))) 
                            then s ++ "Error: component " ++ (printCName a) ++ " not in aggregate " 
                                     ++ n ++ " depends on " ++ (printCName b) 
                                     ++ " inside the aggregate\n\tSuggestion: just depend on " ++ n ++ "\n"
                            else s

-- Ensure there are no dependencies from inside aggregate to outside and vice-versa
aggsErrors :: [Component] -> [Dep] -> String
aggsErrors as ds = foldr aggErrs "" as
    where aggErrs a s = s ++ (aggOut a ds) ++ (aggIn a ds)

-- Highest-level node
data CFuse = CFuse [Stmt] deriving (Eq, Show, Data, Typeable)

aggTop = agg "all" []

aggregatesIn as c = filter (\(CA _ cs _ _) -> elem c cs) as
inAggregate as c  = length (aggregatesIn as c) > 0

-- First argument is the list of aggregate components.  Yields the
-- path of aggregates this component is in.
aggPath :: [Component] -> Component -> [Component]
aggPath as c = let
    aggC  c' = let a = (aggregatesIn as c')
               in if (a == []) then aggTop else (a !! 0)
    aggP  c' = let a = (aggC c') 
               in if a == aggTop then [] else ((aggP a) ++ [a])
    in aggP c

aggPathS :: [Component] -> Component -> String
aggPathS as c = concat (map str p)
    where 
      p     = aggPath as c
      str c = (case c of (CA (CName _ n) _ _ _) -> n) ++ "."

-- How do we find a system-wide free variable?  This is the easiest
-- way, without threading data everywhere, in fact a technique the ghc
-- uses itself.  I'm, of course, going to hell for using
-- unsafePerformIO, but everything's a trade-off.
counter = unsafePerformIO $ newIORef 0
newUniqName _ = unsafePerformIO $
                do
                  i <- readIORef counter
                  writeIORef counter (i+1)
                  return $ show i

progConcat :: CFuse -> CFuse -> CFuse
progConcat (CFuse p1) (CFuse p2) = CFuse (p1 ++ p2)

-- Querying the structure
lstCs :: CFuse -> [Component]
lstCs p = let f = (\c -> case c of 
                           (CS _ _ _ _ _) -> True
                           _              -> False)
          in nub $ listify f p

lstOnlyCs :: CFuse -> [Component]
lstOnlyCs = lstCs

lstAggs :: CFuse -> [Component]
lstAggs p = let f = (\c -> case c of
                             (CA _ _ _ _) -> True 
                             _            -> False)
            in nub $ listify f p

lstDeps :: CFuse -> [Dep]
lstDeps p = let f = (\c -> case c of (Dep _ _) -> True)
            in nub $ listify f p

lstNADeps :: CFuse -> [Dep]
lstNADeps p = let f = (\c -> case c of 
                               (Dep (CA _ _ _ _) _) -> False
                               (Dep _ (CA _ _ _ _)) -> False
                               (Dep _ _ )           -> True)
              in nub $ listify f p

lstIFs :: CFuse -> [Component]
lstIFs p = let f = (\c -> case c of
                            (CR _ _) -> True
                            _        -> False)
           in nub $ listify f p


--[ Graph Backend ]--

compIndex :: [Component] -> Component -> Int
compIndex cs c = n
    where a = elemIndex c cs
          n = case a of 
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

compGraphFlat :: CFuse -> Gr Component String
compGraphFlat p = let cs = lstCs p
                      vs = cmkVertices cs
                      es = cmkEdges (lstNADeps p) (compIndex cs)
                  in mkGraph vs es

compGraphAgg :: CFuse -> Gr Component String
compGraphAgg p = let cs = (lstCs p) ++ (lstAggs p)
                     vs = cmkVertices cs
                     es = cmkEdges (lstDeps p) (compIndex cs)
                 in mkGraph vs es

--cParams = GraphvizParams n nl el cl nl
cParams aggfn = defaultParams { clusterBy = cb
                              , fmtNode   = fn
                              , fmtEdge   = fe}
    where 
      cb (i, c)                               = C (aggfn c) (N (i, c))
      fn (_, (CS (CName s1 s2) _ _ u _))      = [toLabel (s1 ++ "." ++ s2 ++ "\n" ++ u)]
      fn (_, (CA (CName _ n) _ u _))          = [toLabel (n ++ " (aggregate)\n" ++ u)]
      fn _                                    = [toLabel ""]
      fe (_, _, e)                            = [toLabel e] -- LHead "cname", LTail

translateToDot :: CFuse -> (CFuse -> Gr Component String) -> String
translateToDot p gg = unpack (printDotGraph $ graphToDot (cParams (aggPathS (lstAggs p))) (gg p))

graphFlat :: CFuse -> String
graphFlat p = translateToDot p compGraphFlat

graphAgg :: CFuse -> String
graphAgg p = translateToDot p compGraphAgg

-- dependency list construction
depl :: [(Component, [Component])] -> [Stmt]
depl dl = concat $ map (\(c, cs) -> map (\d -> SDep $ dep c d) cs) dl

deps2Stmt ds = map (\d -> SDep d) ds

cnames :: [Component] -> [CName]
cnames cs = map (\c -> nameC c) cs

nameC :: Component -> CName
nameC (CS n _ _ _ _) = n
nameC (CA n _ _ _)   = n
nameC _              = CName "" ""

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
       (\e -> do {perror ("Error: no component named " ++ (name2str "." n) ++ 
                          "\n\tSuggestion: perhaps run 'make' in src/"); return "";})
  d <- (readFile $ implDir ++ (name2str "/" n) ++ fnDeps) `catch` 
       (\e -> do {perror ("Error: no component named " ++ (name2str "." n) ++ 
                          "\n\tSuggestion: perhaps run 'make' in src/"); return "";})
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

isSystemAgg (CA (CName _ n) _ _ _) = n == "system" 
isSystemAgg _                      = False

-- adds in the top "system" aggregate component
cfuse :: [Stmt] -> CFuse
cfuse ss = n
    where p  = CFuse ss
          as = lstAggs p
          cs = lstCs p
          ds = lstDeps p
          na = filter (\c -> not $ inAggregate as c) (cs ++ as)
          n  = if ((not (length na == 1)) || (not (isSystemAgg (na !! 0)))) then 
                   CFuse $ ([SComp (agg "system" na)]) ++ (deps2Stmt ds)
               else p -- has the system aggregate already been added?

-- utility functions for printing
aggStructS p = foldr (\c s -> (printCName c) ++ "\t" ++ (aggPathS as c) ++ "\n" ++ s) "" (lstCs p)
    where as = lstAggs p
aggMembersS p = aggMs $ lstAggs p
    where members (CA _ cs _ _) = concatMap (\c -> (printCName c) ++ ", ") cs
          aggMs as              = concatMap (\a -> (printCName a) ++ ": " ++ (members a) ++ "\n") as

outputFlat :: CFuse -> (CFuse -> String) -> IO(String)
outputFlat p gen = 
    let ifs = ifns p
    in do 
      --         args  <- getArgs
      ifnfo <- readAllIFfns ifs
      cnfo  <- readAllCTypes (cnames (lstOnlyCs p))
      let tdb       = constructTdb p cnfo ifnfo
          deps      = lstDeps p
          aggs      = lstAggs p
          err       = (stringifyUndefDeps tdb p) ++ 
                      (aggSingularS aggs ((lstCs p) ++ aggs)) ++
                      (aggsErrors (lstAggs p) (lstDeps p))
          moreds as = generateAllAggDeps tdb deps as
       --             newds p   = (deps \\ (depsWith aggs deps)) ++ (moreds aggs)
          newds p   = deps ++ (moreds aggs)
          newprog p = cfuse (deps2Stmt (newds p))
       --         hPutStrLn stdout $ aggStructS p
          out = if err == "" then (gen (newprog p)) else err
      return out

outputFlatGraph :: CFuse -> IO(String)
outputFlatGraph p = outputFlat p graphFlat

-- compStr :: Component -> String
-- compStr c@(CS n a o _) = loaded ++ init ++ (printCName n) ++ ".o," ++ schedInit ++ ";"
--     where schedInit
--           loaded = if (not $ cIsInit c) then "!" else ""
--           init   = if (cIsSched c) then "*" else ""

-- runscriptFlat :: CFuse -> String
-- runscriptFlat p = out
--     where
--       cs   = lstCs p
--       ds   = lstDeps p
--       cStr = map compStr cs
--       dStr = 

-- outputFlatRunscript :: CFuse -> IO(String)
-- outputFlatRunscript p = outputFlat p runscriptFlat

outputAggGraph :: CFuse -> IO(String)
outputAggGraph p = 
    let ifs = ifns p
    in do 
      --         args  <- getArgs
      ifnfo <- readAllIFfns ifs
      cnfo  <- readAllCTypes (cnames (lstOnlyCs p))
      let tdb       = constructTdb p cnfo ifnfo
          deps      = lstDeps p
          aggs      = lstAggs p
          err       = (stringifyUndefDeps tdb p) ++ 
                      (aggSingularS aggs ((lstCs p) ++ aggs)) ++
                      (aggsErrors aggs deps)
          out = if err == "" then (graphAgg p) else err
      return out

program = sysAgg

main :: IO ()
main = let p   = cfuse $ concat program
       in do 
         out <- outputAggGraph p
         putStrLn out

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
             boot   = c "no_interface" "boot"
             ll     = agg "ll" [c0, fprr, mm, print, schedconf, st, bc, cg, boot]

             mpool  = c "mem_pool" "naive"
             sm     = c "stkmgr" "naive"
             l      = c "lock" "two_phase"
             e      = c "evt" "edge"
             te     = c "timed_blk" "timed_evt"
             stat   = c "no_interface" "stat"
             buf    = c "cbuf_c" "naive"
             tp     = c "no_interface" "tmem_policy"
             va     = c "valloc" "simple"
             tmem   = agg "tmem" [mpool, sm, l, e, te, stat, buf, tp, va]
--             tmem2  = dup tmem
             ucbuf1 = c "tests" "unit_cbuf1"
             ucbuf2 = c "tests" "unit_cbuf2"
             ucbuf1d = dup ucbuf1
             ucbuf2d = dup ucbuf2
         in [depl [ (c0, [fprr])
                  , (fprr, [print, mm, st, schedconf, bc])
                  , (bc, [print])
                  , (mm, [print])
                  , (cg, [fprr])
                  , (st, [print])
                  , (schedconf, [print])
                  , (boot, [print, fprr, mm, cg])

                  , (tmem, [ll])
--                  , (tmem2, [ll])
                  , (l, [])
                  , (te, [sm, va])
                  , (e, [sm, l, va])
                  , (stat, [sm, te, l, e])
                  , (sm, [va, l, mpool])
                  , (buf, [sm, l, va, mpool])
                  , (mpool, [va, l])
                  , (tp, [sm, buf, te, va, mpool])
                  , (va, [l])

                  -- , (l, [ll])
                  -- , (te, [sm, ll, va])
                  -- , (e, [sm, ll, l, va])
                  -- , (stat, [sm, te, ll, l, e])
                  -- , (sm, [ll, va, l, mpool])
                  -- , (buf, [sm, l, ll, va, mpool])
                  -- , (mpool, [ll, va, l])
                  -- , (tp, [sm, buf, te, ll, va, mpool])
                  -- , (va, [ll, l])

                  , (ucbuf1, [ll, tmem, ucbuf2])
                  , (ucbuf2, [ll, tmem])
                  , (ucbuf1d, [ll, tmem, ucbuf2d])
                  , (ucbuf2d, [ll, tmem])
                  ]]
