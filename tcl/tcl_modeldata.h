
static int ModelData_Cmd (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	Tcl_Obj *res;
	char tmp_str[1024];
	char data[10000];
	data[0] = 0;

	sprintf(tmp_str, "{{name} {%s}} ", ModelData[ModelActive].name); strcat(data, tmp_str);
	sprintf(tmp_str, "{{image} {%s}} ", ModelData[ModelActive].image); strcat(data, tmp_str);
	sprintf(tmp_str, "{{modeltype} {%i}} ", ModelData[ModelActive].modeltype); strcat(data, tmp_str);
	sprintf(tmp_str, "{{teletype} {%i}} ", ModelData[ModelActive].teletype); strcat(data, tmp_str);
	sprintf(tmp_str, "{{teledevice} {%s}} ", ModelData[ModelActive].teledevice); strcat(data, tmp_str);
	sprintf(tmp_str, "{{telebaud} {%i}} ", ModelData[ModelActive].telebaud); strcat(data, tmp_str);
	sprintf(tmp_str, "{{telebtaddr} {%s}} ", ModelData[ModelActive].telebtaddr); strcat(data, tmp_str);
	sprintf(tmp_str, "{{telebtpin} {%s}} ", ModelData[ModelActive].telebtpin); strcat(data, tmp_str);
	sprintf(tmp_str, "{{mode} {%i}} ", ModelData[ModelActive].mode); strcat(data, tmp_str);
	sprintf(tmp_str, "{{status} {%i}} ", ModelData[ModelActive].status); strcat(data, tmp_str);
	sprintf(tmp_str, "{{armed} {%i}} ", ModelData[ModelActive].armed); strcat(data, tmp_str);
	sprintf(tmp_str, "{{heartbeat} {%i}} ", ModelData[ModelActive].heartbeat); strcat(data, tmp_str);
	sprintf(tmp_str, "{{heartbeat_rc} {%i}} ", ModelData[ModelActive].heartbeat_rc); strcat(data, tmp_str);
	sprintf(tmp_str, "{{found_rc} {%i}} ", ModelData[ModelActive].found_rc); strcat(data, tmp_str);
	sprintf(tmp_str, "{{p_lat} {%f}} ", ModelData[ModelActive].p_lat); strcat(data, tmp_str);
	sprintf(tmp_str, "{{p_long} {%f}} ", ModelData[ModelActive].p_long); strcat(data, tmp_str);
	sprintf(tmp_str, "{{p_alt} {%f}} ", ModelData[ModelActive].p_alt); strcat(data, tmp_str);
	sprintf(tmp_str, "{{alt_offset} {%f}} ", ModelData[ModelActive].alt_offset); strcat(data, tmp_str);
	sprintf(tmp_str, "{{baro} {%f}} ", ModelData[ModelActive].baro); strcat(data, tmp_str);
	sprintf(tmp_str, "{{pitch} {%f}} ", ModelData[ModelActive].pitch); strcat(data, tmp_str);
	sprintf(tmp_str, "{{roll} {%f}} ", ModelData[ModelActive].roll); strcat(data, tmp_str);
	sprintf(tmp_str, "{{yaw} {%f}} ", ModelData[ModelActive].yaw); strcat(data, tmp_str);
	sprintf(tmp_str, "{{speed} {%f}} ", ModelData[ModelActive].speed); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage} {%f}} ", ModelData[ModelActive].voltage); strcat(data, tmp_str);
	sprintf(tmp_str, "{{load} {%f}} ", ModelData[ModelActive].load); strcat(data, tmp_str);
	sprintf(tmp_str, "{{gpsfix} {%i}} ", ModelData[ModelActive].gpsfix); strcat(data, tmp_str);
	sprintf(tmp_str, "{{numSat} {%i}} ", ModelData[ModelActive].numSat); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(0)} {%i}} ", ModelData[ModelActive].radio[0]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(1)} {%i}} ", ModelData[ModelActive].radio[1]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(2)} {%i}} ", ModelData[ModelActive].radio[2]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(3)} {%i}} ", ModelData[ModelActive].radio[3]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(4)} {%i}} ", ModelData[ModelActive].radio[4]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(5)} {%i}} ", ModelData[ModelActive].radio[5]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(6)} {%i}} ", ModelData[ModelActive].radio[6]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(7)} {%i}} ", ModelData[ModelActive].radio[7]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(8)} {%i}} ", ModelData[ModelActive].radio[8]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(9)} {%i}} ", ModelData[ModelActive].radio[9]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(10)} {%i}} ", ModelData[ModelActive].radio[10]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(11)} {%i}} ", ModelData[ModelActive].radio[11]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(12)} {%i}} ", ModelData[ModelActive].radio[12]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(13)} {%i}} ", ModelData[ModelActive].radio[13]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(14)} {%i}} ", ModelData[ModelActive].radio[14]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{radio(15)} {%i}} ", ModelData[ModelActive].radio[15]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{acc_x} {%f}} ", ModelData[ModelActive].acc_x); strcat(data, tmp_str);
	sprintf(tmp_str, "{{acc_y} {%f}} ", ModelData[ModelActive].acc_y); strcat(data, tmp_str);
	sprintf(tmp_str, "{{acc_z} {%f}} ", ModelData[ModelActive].acc_z); strcat(data, tmp_str);
	sprintf(tmp_str, "{{gyro_x} {%f}} ", ModelData[ModelActive].gyro_x); strcat(data, tmp_str);
	sprintf(tmp_str, "{{gyro_y} {%f}} ", ModelData[ModelActive].gyro_y); strcat(data, tmp_str);
	sprintf(tmp_str, "{{gyro_z} {%f}} ", ModelData[ModelActive].gyro_z); strcat(data, tmp_str);
	sprintf(tmp_str, "{{rssi_rx} {%i}} ", ModelData[ModelActive].rssi_rx); strcat(data, tmp_str);
	sprintf(tmp_str, "{{rssi_tx} {%i}} ", ModelData[ModelActive].rssi_tx); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_rx} {%f}} ", ModelData[ModelActive].voltage_rx); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_zell(0)} {%f}} ", ModelData[ModelActive].voltage_zell[0]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_zell(1)} {%f}} ", ModelData[ModelActive].voltage_zell[1]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_zell(2)} {%f}} ", ModelData[ModelActive].voltage_zell[2]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_zell(3)} {%f}} ", ModelData[ModelActive].voltage_zell[3]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_zell(4)} {%f}} ", ModelData[ModelActive].voltage_zell[4]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{voltage_zell(5)} {%f}} ", ModelData[ModelActive].voltage_zell[5]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{temperature(0)} {%i}} ", ModelData[ModelActive].temperature[0]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{temperature(1)} {%i}} ", ModelData[ModelActive].temperature[1]); strcat(data, tmp_str);
	sprintf(tmp_str, "{{ampere} {%f}} ", ModelData[ModelActive].ampere); strcat(data, tmp_str);

	res = Tcl_NewStringObj(data, -1);
	Tcl_SetObjResult (interp, res);
	return TCL_OK;
}


