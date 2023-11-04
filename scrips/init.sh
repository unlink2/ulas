#!/bin/bash

# rename to project specific values
NAME="ulas"
CONST="ULAS"
STRUCT="ulas"
FN="ulas"

# will be replaced
ULAS_NAME="ulas"
ULAS_STRUCT="ulas"
ULAS_CONST="ULAS"
ULAS_FN="ulas"

function replace() {
	echo "Replacing $1 with $2"
	find ./ -type f -name '*.c' -exec sed -i "s/$1/$2/g" {} \;
	find ./ -type f -name '*.h' -exec sed -i "s/$1/$2/g" {} \;
	find ./ -type f -name '*.md' -exec sed -i "s/$1/$2/g" {} \;
	find ./ -type f -name 'makefile' -exec sed -i "s/$1/$2/g" {} \;
	find ./ -type f -name '*.sh' -exec sed -i "s/$1/$2/g" {} \;
	find ./doc/ -type f -name '*' -exec sed -i "s/$1/$2/g" {} \;
}

replace $ULAS_NAME $NAME
replace $ULAS_CONST $CONST
replace $ULAS_STRUCT $STRUCT
replace $ULAS_FN $FN

mv "include/$ULAS_NAME.h" "include/$NAME.h"
mv "src/$ULAS_NAME.c" "src/$NAME.c"
mv "doc/$ULAS_NAME.man" "doc/$NAME.man" 
