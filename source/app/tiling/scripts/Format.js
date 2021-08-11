.pragma library

var lc = Qt.locale("C")

function integerStr(value) {
	return value != null
		? "%1".arg(Number(value).toLocaleString(lc, "f", 0))
		: "-"
}

