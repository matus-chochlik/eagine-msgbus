.pragma library

var lc = Qt.locale("C")

function percentStr(value) {
	return value
		? "%1 %".arg(Number(value*100.0).toLocaleString(lc, "f", 1))
		: "-"
}

function integerStr(value) {
	return value != null
		? "%1".arg(Number(value).toLocaleString(lc, "f", 0))
		: "-"
}

