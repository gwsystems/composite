{-# LANGUAGE DeriveDataTypeable, NoMonomorphismRestriction #-}

{- 
TODO:
- check for cycles
- runscript backend
- check for dependencies that resolve to an ambiguous exporter
- check that two componenents in the same aggregate don't define
  overlapping functions when there is a dependency on the aggregate
  (specific case of the previous bullet)
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
               | CA CName [Component] [Dep] Uniq (Maybe Component)
               -- Aggregate: Name, Components in aggregate, and a pointer to the original (see above)
               | CR Component [IFName] deriving (Eq, Show, Data, Typeable)
               -- Restricted interface: component to be restricted, and the list of interfaces restricted to

-- The name has an interface, implementation, and possibly a duplicated name
data CName     = CName String String deriving (Eq, Show, Data, Typeable)

data COptions  = COpt { isSched :: Bool
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

--[ System construction utility functions ]--
ca :: String -> String -> String -> Component
ca i n a = CS (CName i n) a COpt {isSched = False, isInit = False} (newUniqName 1) Nothing

c :: String -> String -> Component
c i n = CS (CName i n) [] COpt {isSched = False, isInit = False} (newUniqName 1) Nothing

cSetArgs   (CS n _ c u orig) s = CS n s c u orig

cSetSched  :: Component -> Component
cSetSched  (CS n c o u orig) = CS n c (o {isSched=True}) u orig
cSetInit   (CS n c o u orig) = CS n c (o {isInit=True}) u orig

agg :: String -> [Component] -> [Dep] -> Component
agg name cs ds = CA (CName "aggregate" name) (nub cs) (nub ds) (newUniqName 1) Nothing

-- OK, the idea here is that we want a _new_ version of the component.
-- Thus, we generate a fresh name for it.  This works because the Eq
-- class that Component derives will make this duplicate non-equal to
-- other verions of the component.
dup :: Component -> Component
dup c@(CS n i o u orig) = CS n i COpt {isSched = isSched o, isInit = isInit o} 
                          (newUniqName 1) (Just (case orig of
                                                   Just c' -> c'
                                                   Nothing -> c))
dup a@(CA n cs ds u orig)  = CA n ncs [] (newUniqName 1) 
                             (Just (case orig of 
                                      (Just c') -> c'
                                      (Nothing) -> a))
    where
      ncs = map dup cs

restrict :: Component -> [String] -> Component
restrict c is = CR c is

dep  c1 c2   = Dep c1 c2

--[ End system constructor utility functions ]-- 

cIsSched :: Component -> Bool
cIsSched   (CS n a o u u') = isSched o
cIsInit    (CS n a o u u') = isInit o

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
compName (CA n _ _ _ _) = n

cexps :: Tdb -> Component -> [Fn]
cexps t c@(CS n _ _ _ _)   = case (queryCT t $ compName c) of T es ds -> es
cexps t   (CR c' is)       = 
    intersect (concatMap (\i -> queryIFT t i) is) (cexps t c')
cexps t   (CA _ cs _ _ _ ) = 
    nub $ concatMap (\c' -> cexps t c') cs

cdeps :: Tdb -> [Dep] -> Component -> [Fn]
cdeps t ds c@(CS n _ _ _ _)  = case (queryCT t $ compName c) of T es ds -> ds
cdeps t ds   (CR c' _)       = cdeps t ds c'
cdeps t ds   (CA _ cs _ _ _) = nub $ concatMap (depsUnsatisfied t ds) cs

--[ Checking for undefined dependencies ]--

-- the exported functions of all depended-on components
depExpList :: Tdb -> Component -> Dep -> [Fn]
depExpList t c d = case d of (Dep a b) -> if a == c then cexps t b else []

depsUnsatisfied :: Tdb -> [Dep] -> Component -> [Fn]
depsUnsatisfied t ds c = (cdeps t ds c) \\ exps
    where exps = concatMap (depExpList t c) ds

-- (type database, components, aggregates, dependencies) -> mapping of
-- deps to undefined functions
undefinedDependencies :: Tdb -> [Component] -> [Component] -> [Dep] -> [(String, [Fn])]
undefinedDependencies t acs as ds = 
    let cs    = acs ++ as
        ns    = map printCName cs
        uds   = map (depsUnsatisfied t ds) cs
        z     = zip ns uds
        undef = filter (\(n, fns) -> not (fns == [])) z
        topA  = (aggPath as (acs !! 0)) !! 0
        isErr = not $ depsUnsatisfied t ds topA == []
    in if (isErr) then undef else []

stringifyUndefDeps :: Tdb -> [Component] -> [Component] -> [Dep] -> String
stringifyUndefDeps t cs as ds =
    let
        pfn = (\(n, fns) s -> s ++ "Error: component " ++ n 
                              ++ " has undefined dependencies:\n\t" ++ (show fns) ++ "\n")
    in foldr pfn "" (undefinedDependencies t cs as ds)

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
generateAggDeps t deps a@(CA n cs _ _ _) = all where
    -- comp dependencies from agg
    lout       = map (\(Dep f t) -> t) $ filter (\(Dep f t) -> f == a) deps 
    newOutDeps = concatMap (\cina -> outDeps t deps cina lout) cs
    -- comp dependencies coming into agg
    lin        = map (\(Dep f t) -> f) $ filter (\(Dep f t) -> t == a) deps
    newInDeps  = concatMap (\cina -> inDeps t deps cina lin) cs
    all        = newInDeps ++ newOutDeps
generateAggDeps t deps a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

generateOutAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateOutAggDeps t deps a@(CA n cs _ _ _) = newOutDeps where
    -- comp dependencies from agg
    lout       = map (\(Dep f t) -> t) $ filter (\(Dep f t) -> f == a) deps 
    newOutDeps = concatMap (\cina -> outDeps t deps cina lout) cs
generateOutAggDeps t deps a = [Dep (c "should pass" "aggregates") (c "into" "generateAggDeps")]

generateInAggDeps :: Tdb -> [Dep] -> Component -> [Dep]
generateInAggDeps t deps a@(CA n cs _ _ _) = newInDeps where
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
    aggs c        = filter (\a@(CA _ cs _ _ _) -> elem c cs) as
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
aggOut (CA (CName _ n) cs _ _ _) ds = foldr chk "" ds
    where chk (Dep a b) s = if ((elem a cs) && (not (elem b cs))) 
                            then s ++ "Error: component " ++ (printCName a) ++ " in aggregate "
                                     ++ n ++ " depends on " ++ (printCName b) 
                                     ++ " outside of the aggregate\n\tSuggestion: just make " ++ n 
                                     ++ " depend on " ++ (printCName b) ++ "\n"
                            else s

-- inbound links?
aggIn (CA (CName _ n) cs _ _ _) ds = foldr chk "" ds
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
data CFuse = CFuse Component deriving (Eq, Show, Data, Typeable)

aggTop = agg "all" [] []

aggregatesIn :: [Component] -> Component -> [Component]
aggregatesIn as c = filter (\(CA _ cs _ _ _) -> elem c cs) as
inAggregate as c  = length (aggregatesIn as c) > 0

-- First argument is the list of aggregate components.  Yields the
-- path of aggregates this component is in.
aggPath :: [Component] -> Component -> [Component]
aggPath as c = let
    aggC  c' = let a = aggregatesIn as c'
               in if (a == []) then aggTop else (a !! 0)
    aggP  c' = let a = aggC c'
               in if a == aggTop then [] else ((aggP a) ++ [a])
    in aggP c

aggPathS :: [Component] -> Component -> String
aggPathS as c = concat (map str p)
    where 
      p     = aggPath as c
      str c = (case c of (CA (CName _ n) _ _ _ _) -> n) ++ "."

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

-- Querying the structure
lstCs :: CFuse -> [Component]
lstCs p = let f = (\c -> case c of 
                           (CS _ _ _ _ _) -> True
                           _              -> False)
          in nub $ listify f p

-- verbose arguments
lstCsV :: [Component] -> [Dep] -> [Component]
lstCsV cs ds = let f = (\c -> case c of 
                                (CS _ _ _ _ _) -> True
                                _              -> False)
            in nub $ (listify f cs) ++ (listify f ds)

lstOnlyCs :: CFuse -> [Component]
lstOnlyCs = lstCs

lstAggs :: CFuse -> [Component]
lstAggs p = let f = (\c -> case c of
                             (CA _ _ _ _ _) -> True 
                             _            -> False)
            in nub $ listify f p

lstAggsV :: [Component] -> [Dep] -> [Component]
lstAggsV cs ds = let f = (\c -> case c of
                                  (CA _ _ _ _ _) -> True 
                                  _            -> False)
              in nub $ (listify f cs) ++ (listify f ds)
               
lstDeps :: CFuse -> [Dep]
lstDeps p = let f = (\c -> case c of (Dep _ _) -> True)
            in nub $ listify f p

lstDepsV :: [Component] -> [Dep] -> [Dep]
lstDepsV cs ds = let f = (\c -> case c of (Dep _ _) -> True)
                 in nub $ (listify f cs) ++ (listify f ds)

lstNADeps :: CFuse -> [Dep]
lstNADeps p = let f = (\c -> case c of 
                               (Dep (CA _ _ _ _ _) _) -> False
                               (Dep _ (CA _ _ _ _ _)) -> False
                               (Dep _ _ )             -> True)
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
                     -- (filter (\c -> not (isDup c)) (lstCs p)) ++ (lstAggs p)
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
      fn (_, (CA (CName _ n) _ _ u _))        = [toLabel (n ++ " (aggregate)\n" ++ u)]
      fn _                                    = [toLabel ""]
      fe (_, _, e)                            = [toLabel e] -- LHead "cname", LTail

translateToDot :: CFuse -> (CFuse -> Gr Component String) -> String
translateToDot p gg = unpack (printDotGraph $ graphToDot (cParams (aggPathS (lstAggs p))) (gg p))

graphFlat :: CFuse -> String
graphFlat p = translateToDot p compGraphFlat

graphAgg :: CFuse -> String
graphAgg p = translateToDot p compGraphAgg

-- dependency list construction
depl :: [(Component, [Component])] -> [Dep]
depl dl = concat $ map (\(c, cs) -> map (\d -> dep c d) cs) dl

cnames :: [Component] -> [CName]
cnames cs = map (\c -> nameC c) cs

nameC :: Component -> CName
nameC (CS n _ _ _ _) = n
nameC (CA n _ _ _ _) = n
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

isSystemAgg (CA (CName _ n) _ _ _ _) = n == "system" 
isSystemAgg _                        = False

-- adds in the top "system" aggregate component
cfuse :: [Component] -> [Dep] -> CFuse
cfuse cs ds = n
    where as  = lstAggsV cs ds
          cs' = lstCsV cs ds
          ds' = lstDepsV cs ds
          na  = filter (\c -> not $ inAggregate as c) (cs' ++ as)
          sys = filter isSystemAgg as
          n   = if (length sys == 0) then 
                    CFuse $ (agg "system" na ds')
                else CFuse $ sys' -- has the system aggregate already been added?
                    where sys' = (agg "system" (ocs (sys !! 0)) ds')
                          ocs (CA _ comps _ _ _) = comps
                                  

-- utility functions for printing
aggStructS p = foldr (\c s -> (printCName c) ++ "\t" ++ (aggPathS as c) ++ "\n" ++ s) "" (lstCs p)
    where as = lstAggs p
aggMembersS p = aggMs $ lstAggs p
    where members (CA _ cs _ _ _) = concatMap (\c -> (printCName c) ++ ", ") cs
          aggMs as                = concatMap (\a -> (printCName a) ++ ": " ++ (members a) ++ "\n") as
depsS ds = concatMap (\(Dep f t) -> "(" ++ (printCName f) ++ ", " ++ (printCName t) ++ ")\n") ds
compsS cs = concatMap (\c -> printCName c ++ "\n") cs

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

--[ Flat graph processing ]--

aggDupOf :: Component -> Maybe Component
aggDupOf (CS _ _ _ _ _)   = Nothing
aggDupOf (CA _ _ _ _ mc)  = mc

isDup :: Component -> Bool
isDup (CS _ _ _ _ (Just c)) = True
isDup (CA _ _ _ _ (Just c)) = True
isDup _                     = False

isDupOf :: Component -> Component -> Bool
isDupOf o d = case d of 
              (CS _ _ _ _ (Just c)) -> c == o
              (CA _ _ _ _ (Just a)) -> a == o
              _ -> False

-- duplicate aggregate, original aggregate it is a duplicate of, all
-- deps, and return the new additional deps
dupAgg :: Component -> Component -> [Dep] -> [Dep]
dupAgg d o ds = let 
    csIn a           = case a of 
                         (CA _ cs _ _ _) -> cs
                         _               -> []
    origCs           = csIn o
    dupCs            = csIn d
    depsIn           = filter (\(Dep f t) -> (elem f origCs) || (elem t origCs)) ds
    getDup c         = (filter (isDupOf c) dupCs) !! 0 -- should be a singleton list
    cpyDep (Dep f t) = Dep (getDup f) (getDup t)
    newds            = map cpyDep depsIn
    in newds ++ (dupAggs d ds) -- the recursion

dupAggs :: Component -> [Dep] -> [Dep]
dupAggs (CS _ _ _ _ _)  ds = []
dupAggs (CA _ cs _ _ _) ds = concatMap (\c -> let a = aggDupOf c in case a of
                                                                       (Just d)  -> dupAgg c d ds
                                                                       (Nothing) -> dupAggs c ds) cs

-- Creating the CD with dup alone doesn't duplicate the dependencies
-- within the aggregate.  Do that here.
duplicateAggs :: Tdb -> CFuse -> [Dep]
duplicateAggs t p = ndeps where
    as    = lstAggs p
    ds    = lstDeps p
    sysa  = (filter isSystemAgg as) !! 0
    ndeps = dupAggs sysa ds

outputFlat :: CFuse -> (CFuse -> String) -> IO(String)
outputFlat p gen = 
    let ifs = ifns p
    in do 
      --         args  <- getArgs
      ifnfo <- readAllIFfns ifs
      cnfo  <- readAllCTypes (cnames (lstOnlyCs p))
      let tdb     = constructTdb p cnfo ifnfo
          aggs    = lstAggs p
          comps   = lstCs p
          dupdeps = duplicateAggs tdb p
          deps    = (lstDeps p) ++ dupdeps
          err     = (stringifyUndefDeps tdb comps aggs deps) ++ 
                    (aggSingularS aggs (comps ++ aggs)) ++
                    (aggsErrors aggs deps)
          newds   = deps ++ (generateAllAggDeps tdb deps aggs)
          newprog = cfuse (comps ++ aggs) newds
          out     = if err == "" then (gen newprog) else err
      return out

outputFlatGraph :: CFuse -> IO(String)
outputFlatGraph p = outputFlat p graphFlat

--[ Aggregate graph processing ]--

outputAggGraph :: CFuse -> IO(String)
outputAggGraph p = 
    let ifs = ifns p
    in do 
      --         args  <- getArgs
      ifnfo <- readAllIFfns ifs
      cnfo  <- readAllCTypes (cnames (lstOnlyCs p))
      let tdb       = constructTdb p cnfo ifnfo
          aggs      = lstAggs p
          comps     = lstCs p
          dupdeps   = duplicateAggs tdb p
          origdeps  = (lstDeps p)
          deps      = origdeps ++ dupdeps
          err       = (stringifyUndefDeps tdb comps aggs deps) ++ 
                      (aggSingularS aggs (comps ++ aggs)) ++
                      (aggsErrors aggs deps)
          out       = if err == "" then (graphAgg p) else err
      return out



ll :: Component
ll = ll'
    where c0     = c "no_interface" "comp0"
          fprr   = c "sched" "fprr"
          mm     = c "mem_mgr" "naive"
          print  = c "printc" "linux_log"
          schedconf = c "sched_conf" "http_config"
          st     = c "stack_trace" "same_pd"
          bc     = c "sched" "base_case"
          cg     = c "cgraph" "static"
          boot   = c "no_interface" "boot"
          ll'    = agg "ll" [c0, fprr, mm, print, schedconf, st, bc, cg, boot] 
                   (depl [( c0, [fprr])
                         , (fprr, [print, mm, st, schedconf, bc])
                         , (bc, [print])
                         , (mm, [print])
                         , (cg, [fprr])
                         , (st, [print])
                         , (schedconf, [print])
                         , (boot, [print, fprr, mm, cg])])

tmem :: Component
tmem = tmem'
    where mpool  = c "mem_pool" "naive"
          sm     = c "stkmgr" "naive"
          l      = c "lock" "two_phase"
          e      = c "evt" "edge"
          te     = c "timed_blk" "timed_evt"
          stat   = c "no_interface" "stat"
          buf    = c "cbuf_c" "naive"
          tp     = c "no_interface" "tmem_policy"
          va     = c "valloc" "simple"
          tmem'  = agg "tmem" [mpool, sm, l, e, te, stat, buf, tp, va] 
                   (depl [ (l, [])
                         , (te, [sm, va])
                         , (e, [sm, l, va])
                         , (stat, [sm, te, l, e])
                         , (sm, [va, l, mpool])
                         , (buf, [sm, l, va, mpool])
                         , (mpool, [va, l])
                         , (tp, [sm, buf, te, va, mpool])
                         , (va, [l])])

sysAgg :: CFuse
sysAgg = let ucbuf1 = c "tests" "unit_cbuf1"
             ucbuf2 = c "tests" "unit_cbuf2"
         in cfuse [tmem, ll, ucbuf1, ucbuf2] 
                (depl [ (ucbuf1, [ll, tmem, ucbuf2])
                      , (ucbuf2, [ll, tmem])
                      , (tmem, [ll])])

main :: IO ()
main = do 
--  out <- outputFlatGraph sysAgg
  out <- outputAggGraph sysAgg
  putStrLn out
