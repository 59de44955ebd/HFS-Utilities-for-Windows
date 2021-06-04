function mac (){
	res=$(echo $*|iconv -f UTF-8-MAC -t MAC)
	echo $res
	#echo -n \"$res\"
# 	echo $res|sed 's/ /\\ /g'
# 	echo $res|sed 's/ /\\ /g'>>cmd.txt
# 	if [[ $res = *" "* ]]; then
# 		echo \"$res\"
# 		echo \"$res\">>cmd.txt
# 	else
# 		echo $res
# 		echo $res>>cmd.txt
# 	fi
}

# cmd=$(echo "$@"|iconv -f UTF-8-MAC -t MAC)
# echo $cmd>>cmd.txt
# eval $cmd
