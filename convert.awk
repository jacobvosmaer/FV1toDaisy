BEGIN { memtop = 0 }
/^ *;/ {
  sub(" *;", "")
  if($0 ~ "[a-z]")
    printf "/* %s */\n", $0
  next
}
$1 == "mem" {
  mem[$2] = memtop
  memtop += $3
  memsize[$2] = $3
  next
}
$1 == "equ" {
  equ[$2] = $3
  next
}
$1 ~ /^[a-z].*:$/ {
  sub(":$", "", $1)
  printf "label(\"%s\")\n", $1
  next
}
/^ *[a-z]/ {
  print
}
END {
  for (m in mem)
    print m, mem[m], mem[m] + memsize[m]
  for (k in equ)
    print k, equ[k]
}
