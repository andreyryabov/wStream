SRV=ubuntu@54.247.87.230
DIR=/srv/wstream

ssh $SRV "mkdir $DIR/logs"
scp -r  src.srv/*.js   $SRV:$DIR
scp -r  src.srv/*.json $SRV:$DIR
