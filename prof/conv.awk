BEGIN {
      FS=","
}
/"FAPP-core"/ { printf "%s,%s,%s,%s,%s,\"1\",\"12\"\n",$1,$2,$3,$4,$5; }
/"LABEL-FAPP-core"/ { print $0; }
!($0 ~ "FAPP-core") { print $0; }


