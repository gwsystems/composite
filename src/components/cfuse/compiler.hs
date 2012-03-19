{-# LANGUAGE DeriveDataTypeable, NoMonomorphismRestriction #-}

{- 
TODO:
- check for cycles
- output aggregates as clusters correctly -- requires enabling dependencies to and from clusters (in dot)
- generate dependencies between components when there are dependencies to and from aggregates
- runscript backend
- check for dependencies that resolve to an ambiguous exporter
- check that two componenents in the same aggregate don't define
  overlapping functions when there is a dependency on the aggregate
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

data Component = CS CName CArgs COptions Uniq -- Component Specification
               | CA CName [Component]         -- Aggregate
               | CR Component [IFName]        -- Restricted interface
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
type Uniq   = String

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
cSetSchedP (CS n c o u) s = CS n c (o {schedParams=s}) u

cSetArgs   (CS n _ c u) s = CS n s c u

cIsSched  :: Component -> Component
cIsSched  (CS n c o u)   = CS n c (o {isSched=True}) u
cIsFltH   (CS n c o u)   = CS n c (o {isFaultHndl=True}) u
cIsInit   (CS n c o u)   = CS n c (o {isInit=True}) u


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
compName (CS n _ _ _) = n
compName (CR c _)     = compName c
compName (CD c _)     = compName c
compName (CA n _)     = n

cexps :: Tdb -> Component -> [Fn]
cexps t c@(CS n _ _ _) = cexpsC t c
cexps t   (CR c' is)   = intersect (concatMap (\i -> queryIFT t i) is) (cexps t c')
cexps t   (CD c' _)    = cexps t c'
cexps t   (CA _ cs)    = foldr (\c' r -> r ++ (cexps t c')) [] cs
cexpsC t c = case (queryCT t $ compName c) of T es ds -> es

cdeps :: Tdb -> Component -> [Fn]
cdeps t c@(CS n _ _ _) = cdepsC t c
cdeps t   (CR c' _)    = cdeps t c'
cdeps t   (CD c' _)    = cdeps t c'
cdeps t   (CA _ cs)    = concatMap (\c' -> depsUnsatisfied t p c') cs
    where p = case t of (Tdb p _ _) -> p
cdepsC t c = case (queryCT t $ compName c) of T es ds -> ds

--[ Checking for undefined dependencies ]--

depList :: Tdb -> Component -> Dep -> [Fn]
depList t c d = case d of (Dep a b) -> if a == c then cexps t b else []

depsUnsatisfied :: Tdb -> CFuse -> Component -> [Fn]
depsUnsatisfied t p c = (cdeps t c) \\ exps
    where exps = concatMap (depList t c) (lstDeps p)

undefinedDependencies :: Tdb -> CFuse -> [(String, [Fn])]
undefinedDependencies t p = 
    let cs    = lstCs p ++ lstAggs p
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

inDeps :: Tdb -> Component -> [Component] -> [Dep]
inDeps t a cs = 
    let
        doesExp c a = not ((intersect (cexps t a) (cdeps t c)) == [])
        needDep     = filter (\c -> doesExp c a) cs 
    in map (\c -> Dep c a) needDep

outDeps :: Tdb -> Component -> [Component] -> [Dep]
outDeps t a cs = 
    let
        doesExp c a = not ((intersect (cexps t c) (cdeps t a)) == [])
        needDep     = filter (\c -> (doesExp c a)) cs 
    in map (\c -> Dep a c) needDep

depsWith cs deps = concatMap (\c -> filter (\(Dep f t) -> t == c || f == c) deps) cs

-- resolveAggDeps :: Tdb -> CFuse -> [(Component, [Fn])]
-- resolveAggDeps t p = nds
--     where 
--       -- isc2a (Dep (CA _ _) (CA _ _)) = False
--       -- isc2a (Dep _        (CA _ _)) = True
--       -- isc2a _                       = False
--       -- cs2as ds                      = filter isc2a ds
--       newp                          = CFuse $ deps2Stmt (lstNADeps p)
--       -- note that the dependencies from depsUnsatisfied should be
--       -- only those from component to aggregate (and not the opposite) as this is executed 
--       unsatisNoA c                  = (c, depsUnsatisfied t newp c)
--       depsOnAggs                    = filter (\(c, fs) -> not (fs == [])) $ map unsatisNoA (lstCs p)

generateAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateAggDeps t deps a@(CA n cs) = all where
    -- comp dependencies from agg
    lout       = map (\(Dep f t) -> t) $ filter (\(Dep f t) -> f == a) deps 
    newOutDeps = concatMap (\cina -> outDeps t cina lout) cs
    -- comp dependencies coming into agg
    lin        = map (\(Dep f t) -> f) $ filter (\(Dep f t) -> t == a) deps
    newInDeps  = concatMap (\cina -> inDeps t cina lin) cs
    all        = newInDeps ++ newOutDeps
generateAggDeps t deps a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

generateOutAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateOutAggDeps t deps a@(CA n cs) = newOutDeps where
    -- comp dependencies from agg
    lout       = map (\(Dep f t) -> t) $ filter (\(Dep f t) -> f == a) deps 
    newOutDeps = concatMap (\cina -> outDeps t cina lout) cs
generateOutAggDeps t deps a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

generateInAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateInAggDeps t deps a@(CA n cs) = newInDeps where
    lin        = map (\(Dep f t) -> f) $ filter (\(Dep f t) -> t == a) deps
    newInDeps  = concatMap (\cina -> inDeps t cina lin) cs
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
    aggs c        = filter (\a@(CA _ cs) -> elem c cs) as
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
ca i n s a = CS (CName i n) a COpt {schedParams = s, isSched = False, isFaultHndl = False, isInit = False} (newDname 1)

c :: String -> String -> Component
c i n = CS (CName i n) [] COpt {schedParams = [], isSched = False, isFaultHndl = False, isInit = False} (newDname 1)

agg :: String -> [Component] -> Component
agg name cs = CA (CName "aggregate" name) cs

aggTop = agg "all" []

aggregatesIn as c = filter (\(CA _ cs) -> elem c cs) as
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
      str c = (case c of (CA (CName _ n) _) -> n) ++ "."

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
dup (CD c s) = dup c
dup c        = CD c (newDname 1)

restrict :: Component -> [String] -> Component
restrict c is = CR c is

-- Dependencies
dep  c1 c2   = Dep c1 c2

progConcat :: CFuse -> CFuse -> CFuse
progConcat (CFuse p1) (CFuse p2) = CFuse (p1 ++ p2)

-- Querying the structure
lstCs :: CFuse -> [Component]
lstCs p = let f = (\c -> case c of 
                           (CS _ _ _ _) -> True
                           (CD _ _)     -> True
                           _            -> False)
          in nub $ listify f p

lstOnlyCs :: CFuse -> [Component]
lstOnlyCs p = let f = (\c -> case c of 
                           (CS _ _ _ _) -> True
                           _            -> False)
          in nub $ listify f p

lstAggs :: CFuse -> [Component]
lstAggs p = let f = (\c -> case c of
                             (CA _ _) -> True 
                             _        -> False)
            in nub $ listify f p

lstDeps :: CFuse -> [Dep]
lstDeps p = let f = (\c -> case c of (Dep _ _) -> True)
            in nub $ listify f p

lstNADeps :: CFuse -> [Dep]
lstNADeps p = let f = (\c -> case c of 
                               (Dep (CA _ _) _) -> False
                               (Dep _ (CA _ _)) -> False
                               (Dep _ _ )       -> True)
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
      cb (i, c)                               = C (aggfn c) (N (i, c))
      fn (_, (CS (CName s1 s2) _ _ _))        = [toLabel (s1 ++ "." ++ s2)]
      fn (_, (CD (CS (CName s1 s2) _ _ _) _)) = [toLabel (s1 ++ "." ++ s2)]
      fn _                                    = [toLabel ""]
      fe (_, _, e)                            = [toLabel e] -- LHead "cname", LTail

translateToDot :: CFuse -> String
translateToDot p =  unpack (printDotGraph $ graphToDot (cParams (aggPathS (lstAggs p))) (compGraph p))

-- dependency list construction
depl :: [(Component, [Component])] -> [Stmt]
depl dl = concat $ map (\(c, cs) -> map (\d -> SDep $ dep c d) cs) dl

deps2Stmt ds = map (\d -> SDep d) ds

cnames :: [Component] -> [CName]
cnames cs = map (\c -> nameC c) cs

nameC :: Component -> CName
nameC (CS n _ _ _) = n
nameC (CA n _)     = n
nameC _            = CName "" ""

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

-- adds in the top aggregate component
cfuse :: [Stmt] -> CFuse
cfuse ss = CFuse [SComp (agg "system" na)]
    where p = CFuse ss
          as = lstAggs p
          cs = lstCs p
          na = filter (\c -> not $ inAggregate as c) (cs ++ as)

-- utility functions for printing
aggStructS p = foldr (\c s -> (printCName c) ++ "\t" ++ (aggPathS as c) ++ "\n" ++ s) "" (lstCs p)
    where as = lstAggs p
aggMembersS p = aggMs $ lstAggs p
    where members (CA _ cs) = concatMap (\c -> (printCName c) ++ ", ") cs
          aggMs as          = concatMap (\a -> (printCName a) ++ ": " ++ (members a) ++ "\n") as

program = sysAgg

main :: IO ()
main = let p   = cfuse $ concat program
           ifs = ifns p
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
             newds p   = (deps \\ (depsWith aggs deps)) ++ (moreds aggs)
             newprog p = cfuse (deps2Stmt (newds p))
         hPutStrLn stdout $ aggStructS p
         if err == "" then (putStrLn (translateToDot (newprog p))) else (hPutStrLn stderr err)

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

             ucbuf1 = c "tests" "unit_cbuf1"
             ucbuf2 = c "tests" "unit_cbuf2"
         in [depl [ (c0, [fprr])
                  , (fprr, [print, mm, st, schedconf, bc])
                  , (bc, [print])
                  , (mm, [print])
                  , (cg, [fprr])
                  , (st, [print])
                  , (schedconf, [print])
                  , (boot, [print, fprr, mm, cg])

                  , (tmem, [ll])
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
                  , (ucbuf2, [ll, tmem])]]
