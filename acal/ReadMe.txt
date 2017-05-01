
LatitudeU=49.52607 LongitudeU=11.51662 TzShift=-3600 # (or -7200)

TZ=UTC0 ./acal.exe -l -a -m $(($(TZ=UTC0 date -d now "+%s")+($TzShift))) "${LatitudeU}" "${LongitudeU}" $((-($TzShift)))

TZ=UTC0 ./acal.exe -l -a -m $(($(date -d "2005-11-22 7:00:00" "+%s")+($TzShift))) "${LatitudeU}" "${LongitudeU}" $((-($TzShift)))
