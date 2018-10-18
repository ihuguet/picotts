#!lua
-----------------------------------------------------------------------------
-- lua script picoloadphones.lua --- creates pkb containing phones table.
--
-- Copyright (C) 2009 SVOX AG. All rights reserved.
-----------------------------------------------------------------------------

-- load pico phones src file and create phones pkb file

-- accepted syntax:
-- - parses line of the following format:
--   :SYM "<sym>" :PROP mapval = <uint8> { , <propname> = <int> }
-- - initial '!' and trailing '!.*' are treated as comments, no '[]'


--- valid property names
propnames = {mapval=0, vowel=0, diphth=0, glott=0, nonsyllvowel=0, syllcons=0}
--- valid property names (that may occur once only)
upropnames = {primstress=0, secstress=0, syllbound=0, wordbound=0, pause=0}


-- init
if #arg ~= 2 then
  print("*** error: wrong number of arguments, must be 2"); return
end
local infile = io.open(arg[1], "r")
if not infile then
  print("*** error: could not open input file: " .. arg[1]); return
end
local outfile = io.open(arg[2], "wb")
if not outfile then
  print("*** error: could not open output file: " .. arg[2]); return
end


-- tables
--- table with symbol name keys (not really used currently)
local syms = {}
--- table with symbol name number keys (specified with property mapval)
local symnrs = {}
--- array of symbol name numer keys used (to check for unique mapvals)
local symnrsused = {}


-- parse input file, build up syms and symnrs tables
for line in infile:lines() do
  if string.match(line, "^%s*!.*$") or string.match(line, "^%s*$") then
    -- discard comment-only lines
  else
    cline = string.gsub(line, "^%s*", "")
    -- get :SYM
    sym = string.match(cline, "^:SYM%s+\"([^\"]-)\"%s+")
    if not sym then
      sym = string.match(cline, "^:SYM%s+'([^']-)'%s+")
    end
    if sym then
      cline = string.gsub(cline, "^:SYM%s+['\"].-['\"]%s+", "")
      -- get :PROP and mapval prop/propval
      propval = string.match(cline, "^:PROP%s+mapval%s*=%s*(%d+)%s*")
      if propval then
	cline = string.gsub(cline, "^:PROP%s+mapval%s*=%s*%d+%s*", "")
	-- construct props table and add first mapval property
	props = {mapval = tonumber(propval)}
	symnr = tonumber(propval)
	if not symnrsused[symnr] then
	  symnrsused[symnr] = true
	else
	  io.write("*** error: mapval values must be unique, ", symnr, "\n")
	  print("line: ", line); return
	end
	-- check if remaining part are comments only
	cline = string.gsub(cline, "^!.*", "")
	while (#cline > 0) do
	  -- try to get next prop/propval and add to props
	  prop, propval = string.match(cline, "^,%s*(%w+)%s*=%s*(%d+)%s*")
	  if prop and propval then
	    cline = string.gsub(cline, "^,%s*%w+%s*=%s*%d+%s*", "")
	    props[prop] = tonumber(propval)
	  else
	    print("*** error: syntax error in property list")
	    print("line: ", line); return
	  end
	  -- cleanup if only comments remaining
	  cline = string.gsub(cline, "^!.*", "")
	end
      else
	print("*** error: no mapval property found")
	print("line: ", line); return
      end
      syms[sym] = props
      symnrs[symnr] = props
    else
      print("*** error: no symbol found")
      print("line: ", line)
      return
    end
  end
end


-- check syms and symnrs

function checksymtable (st)
  for s in pairs(propnames) do propnames[s] = 0 end
  for s in pairs(upropnames) do upropnames[s] = 0 end
  for s, p in pairs(st) do
    for prop, propval in pairs(p) do
      if not propnames[prop] and not upropnames[prop] then
	io.write("*** error: invalid property name '", prop, "'\n")
	return
      end
      if propnames[prop] then
	propnames[prop] = propnames[prop] + 1
      elseif upropnames[prop] then
	upropnames[prop] = upropnames[prop] + 1
      end
    end
    for prop, propval in pairs(upropnames) do
      if propval > 1  then
	io.write("*** error: property '", prop, "' must be unique\n"); return
      end
    end
  end
end

checksymtable(syms)
checksymtable(symnrs)


-- get IDs of unique specids

specid = {}
for i = 1, 8 do specid[i] = 0 end
for s, pl in pairs(symnrs) do
  if pl["primstress"] then    specid[1] = pl["mapval"]
  elseif pl["secstress"] then specid[2] = pl["mapval"]
  elseif pl["syllbound"] then specid[3] = pl["mapval"]
  elseif pl["pause"] then     specid[4] = pl["mapval"]
  elseif pl["wordbound"] then specid[5] = pl["mapval"]
  end
end


-- write out Phones pkb

function encodeprops (n)
  rv = 0
  pl = symnrs[n]
  if pl then
    if pl["vowel"] then rv = 1 end
    if pl["diphth"]then rv = rv + 2 end
    if pl["glott"] then rv = rv + 4 end
    if pl["nonsyllvowel"] then rv = rv + 8 end
    if pl["syllcons"] then rv = rv + 16 end
  end
  return rv
end

for i=1,8 do
  if specid[i] == 0 then outfile:write("\0")
  else outfile:write(string.format("%c", specid[i]))
  end
end
for i = 0, 255 do
  nr = encodeprops(i)
  if nr == 0 then outfile:write("\0")
  else outfile:write(string.format("%c", nr))
  end
end


-- tini

infile:close()
outfile:close()

-- end
