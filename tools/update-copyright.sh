#!/bin/bash

# Tool to update copyright notices in all source files to reflect the last Git changes.
# Must be called from source root.


FILES=`find . -path "*/.git" -prune -o -path "*/*.nogit" -prune -o -name "update-copyright.sh" -prune -o -path "*/external" -prune -o -path "*/home2l-api*" -prune -o -path "./brownies/circuits" -prune -o -type f -print | grep -v -E "\.(apk|svg|jpg|png|bmp|gz|pdf|bak|log|pot|mo)$" | grep -v "~$"`
for FILE in $FILES; do
  #~ echo FILE = $FILE
  NOTICE=`grep -E "\(C\) .+ Gundolf Kiefer" $FILE`
  #~ echo NOTICE = $NOTICE
  if [ "$NOTICE" != ""  ]; then
    YEARS="`git log --date=format:%Y $FILE | grep '^Date' | sed "s#^[^0-9]*##"`"
    FIRST_GIT=`echo "$YEARS" | tail -1`
    LAST=`echo "$YEARS" | head -1`
    FIRST=`echo "$NOTICE" | sed 's#^[^0-9]*\([0-9]*\).*$#\1#'`
    if [[ "$FIRST" == "$LAST" ]]; then
      NEW="(C) $FIRST Gundolf Kiefer"
    else
      NEW="(C) $FIRST-$LAST Gundolf Kiefer"
    fi
    #~ echo FIRST=$FIRST LAST=$LAST
    #~ echo $NEW
    NOTICE_NEW=`sed "s#(C) .* Gundolf Kiefer#$NEW#" $FILE | grep -E "\(C\) .* Gundolf Kiefer"`
    if [[ "$NOTICE_NEW" != "$NOTICE" ]]; then
      echo "$FILE: '$NOTICE' -> '$NOTICE_NEW'"
      sed -i "s#(C) .* Gundolf Kiefer#$NEW#" $FILE
    fi
  fi
done
