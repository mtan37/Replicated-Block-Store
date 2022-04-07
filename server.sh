while true
do
    echo "running ./server " $@
    ./server $@    
    echo ""
    echo ""
    echo "*****************"
    echo "Restarting server"
    echo "*****************"
    echo ""
    sleep 3
done