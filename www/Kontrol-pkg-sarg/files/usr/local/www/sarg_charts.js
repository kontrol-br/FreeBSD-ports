(function () {
	"use strict";

	var palette = ["#2563eb", "#0d9488", "#f59e0b", "#f97316", "#8b5cf6", "#e11d48", "#0891b2"];

	function text(node) {
		return (node && node.textContent ? node.textContent : "").replace(/\s+/g, " ").trim();
	}

	function number(value) {
		var clean = String(value || "").trim().toUpperCase();
		var multiplier = /T(?:B)?$/.test(clean) ? Math.pow(1024, 4) : /G(?:B)?$/.test(clean) ? Math.pow(1024, 3) :
			/M(?:B)?$/.test(clean) ? Math.pow(1024, 2) : /K(?:B)?$/.test(clean) ? 1024 : 1;
		clean = clean.replace(/[^0-9,.-]/g, "");
		if (/^\d{1,3}(\.\d{3})+(,\d+)?$/.test(clean)) {
			clean = clean.replace(/\./g, "").replace(",", ".");
		} else if (/^\d{1,3}(,\d{3})+(\.\d+)?$/.test(clean)) {
			clean = clean.replace(/,/g, "");
		} else {
			clean = clean.replace(",", ".");
		}
		return (parseFloat(clean) || 0) * multiplier;
	}

	function compactCount(value) {
		if (value >= 1e9) return (value / 1e9).toFixed(1) + "G";
		if (value >= 1e6) return (value / 1e6).toFixed(1) + "M";
		if (value >= 1e3) return (value / 1e3).toFixed(1) + "K";
		return Math.round(value).toLocaleString("pt-BR");
	}

	function formatBytes(value) {
		var units = ["B", "KB", "MB", "GB", "TB"];
		var unit = 0;
		value = Math.max(0, Number(value) || 0);
		while (value >= 1024 && unit < units.length - 1) {
			value /= 1024;
			unit++;
		}
		var decimals = unit === 0 || value >= 100 ? 0 : value >= 10 ? 1 : 2;
		return value.toLocaleString("pt-BR", { minimumFractionDigits: 0, maximumFractionDigits: decimals }) + " " + units[unit];
	}

	function formatByteColumns(table) {
		var headers = table.querySelectorAll("thead th");
		if (!headers.length) return;
		var byteColumns = [];
		Array.prototype.forEach.call(headers, function (header, index) {
			var label = text(header);
			if (!/%/.test(label) && /BYTES?|TR.FEGO|VOLUME|DADOS/i.test(label)) byteColumns.push(index);
		});
		if (!byteColumns.length) return;

		Array.prototype.forEach.call(table.querySelectorAll("tbody tr, tfoot tr"), function (row) {
			var cells = row.querySelectorAll("td, th");
			byteColumns.forEach(function (index) {
				if (!cells[index] || cells[index].dataset.sargBytesFormatted === "1") return;
				var original = text(cells[index]);
				if (original === "" || /%/.test(original)) return;
				var bytes = number(original);
				cells[index].dataset.sargBytesFormatted = "1";
				cells[index].title = "Valor original: " + original;
				cells[index].textContent = formatBytes(bytes);
			});
		});
	}

	function isZeroTimeValue(value) {
		value = String(value || "").replace(/\s+/g, "").replace(/%$/, "");
		return value === "" || /^0+(?:[.:]0+)*$/.test(value);
	}

	function hideUnavailableE2guardianTimeColumns(table) {
		if (!/E2guardian/i.test(document.title + " " + text(document.body))) return;
		var headers = table.querySelectorAll("thead th");
		if (!headers.length) return;
		var bodyRows = table.querySelectorAll("tbody tr");
		Array.prototype.forEach.call(headers, function (header, index) {
			var label = text(header).toUpperCase();
			if (!/^(?:ELAPSED TIME|USED TIME|MILLISEC|%TIME)$/.test(label)) return;
			var hasValue = false;
			var allZero = true;
			Array.prototype.forEach.call(bodyRows, function (row) {
				var cells = row.querySelectorAll("td, th");
				if (!cells[index]) return;
				hasValue = true;
				if (!isZeroTimeValue(text(cells[index]))) allZero = false;
			});
			if (!hasValue || !allZero) return;
			header.style.display = "none";
			header.setAttribute("aria-hidden", "true");
			Array.prototype.forEach.call(table.querySelectorAll("tbody tr, tfoot tr"), function (row) {
				var cells = row.querySelectorAll("td, th");
				if (cells[index]) {
					cells[index].style.display = "none";
					cells[index].setAttribute("aria-hidden", "true");
				}
			});
		});
	}

	function modernizeCalendarIcons() {
		Array.prototype.forEach.call(document.querySelectorAll('img[src$="datetime.png"], img[title*="date/time report"]'), function (image) {
			var icon = document.createElement("span");
			icon.className = "sarg-report-icon sarg-report-icon--calendar";
			icon.setAttribute("aria-hidden", "true");
			icon.innerHTML = '<svg viewBox="0 0 24 24" focusable="false" aria-hidden="true"><path d="M7 2v3M17 2v3M4 8h16M5 4h14a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1Z"/><path d="M8 12h2v2H8zM14 12h2v2h-2zM8 16h2v2H8zM14 16h2v2h-2z" class="sarg-report-icon__days"/></svg>';
			image.parentNode.replaceChild(icon, image);
		});
	}

	function columnIndex(headers, expression) {
		for (var i = 0; i < headers.length; i++) {
			if (expression.test(headers[i])) return i;
		}
		return -1;
	}

	function extract(table) {
		var headerNodes = table.querySelectorAll("thead th");
		var headers = Array.prototype.map.call(headerNodes, text);
		var labelIndex = columnIndex(headers, /USER|USU.A?RIO|SITE|URL|HOST|IP/i);
		var accessIndex = columnIndex(headers, /CONNECT|ACESS|HIT|REQUISI/i);
		var bytesIndex = columnIndex(headers, /BYTES|TR.FEGO|VOLUME|DADOS/i);
		if (labelIndex < 0 || accessIndex < 0 || bytesIndex < 0) return [];
		var rows = table.querySelectorAll("tbody tr");
		var result = [];

		Array.prototype.forEach.call(rows, function (row) {
			var cells = row.querySelectorAll("td");
			if (!cells.length) return;
			var fallbackLabel = row.querySelector("td.data2") || cells[0];
			var li = labelIndex >= 0 && cells[labelIndex] ? labelIndex : Array.prototype.indexOf.call(cells, fallbackLabel);
			var ai = accessIndex;
			if (ai < 0 || !cells[ai]) {
				for (var i = li + 1; i < cells.length; i++) {
					if (number(text(cells[i])) > 0) { ai = i; break; }
				}
			}
			var bi = bytesIndex >= 0 && cells[bytesIndex] ? bytesIndex : ai;
			var label = text(cells[li] || fallbackLabel);
			var accesses = ai >= 0 && cells[ai] ? number(text(cells[ai])) : 0;
			var bytes = bi >= 0 && cells[bi] ? number(text(cells[bi])) : accesses;
			if (label && (accesses > 0 || bytes > 0)) result.push({ label: label, accesses: accesses, bytes: bytes });
		});
		return result;
	}

	function element(name, className, value) {
		var node = document.createElement(name);
		if (className) node.className = className;
		if (value !== undefined) node.textContent = value;
		return node;
	}

	function bars(data) {
		var chart = element("section", "sarg-chart");
		chart.appendChild(element("h3", "", "Acessos por origem"));
		if (!data.length) {
			chart.appendChild(element("p", "sarg-chart-empty", "Sem acessos registrados"));
			return chart;
		}
		var max = Math.max.apply(null, data.map(function (item) { return item.accesses; }));
		data.forEach(function (item, index) {
			var value = item.accesses;
			var row = element("div", "sarg-bar-row");
			row.title = item.label + ": " + compactCount(value);
			row.appendChild(element("span", "sarg-bar-label", item.label));
			var track = element("span", "sarg-bar-track");
			var fill = element("span", "sarg-bar-fill");
			fill.style.width = Math.max(3, value / max * 100) + "%";
			fill.style.backgroundColor = palette[index % palette.length];
			track.appendChild(fill);
			row.appendChild(track);
			row.appendChild(element("span", "sarg-bar-value", compactCount(value)));
			chart.appendChild(row);
		});
		return chart;
	}

	function piePath(cx, cy, radius, start, end) {
		var x1 = cx + radius * Math.cos(start), y1 = cy + radius * Math.sin(start);
		var x2 = cx + radius * Math.cos(end), y2 = cy + radius * Math.sin(end);
		return "M " + cx + " " + cy + " L " + x1 + " " + y1 + " A " + radius + " " + radius + " 0 " +
			(end - start > Math.PI ? 1 : 0) + " 1 " + x2 + " " + y2 + " Z";
	}

	function pie(data) {
		var chart = element("section", "sarg-chart");
		chart.appendChild(element("h3", "", "Participação no tráfego"));
		var pieData = data.filter(function (item) { return item.bytes > 0; }).slice(0, 5);
		if (!pieData.length) {
			chart.appendChild(element("p", "sarg-chart-empty", "Sem volume registrado"));
			return chart;
		}
		var layout = element("div", "sarg-pie-layout");
		var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
		svg.setAttribute("viewBox", "0 0 120 120");
		svg.setAttribute("class", "sarg-pie");
		svg.setAttribute("role", "img");
		var total = pieData.reduce(function (sum, item) { return sum + item.bytes; }, 0);
		var angle = -Math.PI / 2;
		var legend = element("div", "sarg-pie-legend");
		pieData.forEach(function (item, index) {
			var value = item.bytes;
			var next = angle + (value / total) * Math.PI * 2;
			if (pieData.length === 1) {
				var circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
				circle.setAttribute("cx", "60");
				circle.setAttribute("cy", "60");
				circle.setAttribute("r", "54");
				circle.setAttribute("fill", palette[index]);
				svg.appendChild(circle);
			} else {
				var path = document.createElementNS("http://www.w3.org/2000/svg", "path");
				path.setAttribute("d", piePath(60, 60, 54, angle, next));
				path.setAttribute("fill", palette[index]);
				svg.appendChild(path);
			}
			angle = next;
			var key = element("div", "sarg-pie-key");
			key.title = item.label + ": " + formatBytes(value);
			var swatch = element("span", "sarg-pie-swatch");
			swatch.style.background = palette[index];
			key.appendChild(swatch);
			key.appendChild(element("span", "", item.label));
			key.appendChild(element("span", "", Math.round(value / total * 100) + "%"));
			legend.appendChild(key);
		});
		layout.appendChild(svg);
		layout.appendChild(legend);
		chart.appendChild(layout);
		return chart;
	}

	function columns(data) {
		var chart = element("section", "sarg-chart");
		chart.appendChild(element("h3", "", "Volume comparado"));
		if (!data.length) {
			chart.appendChild(element("p", "sarg-chart-empty", "Sem volume registrado"));
			return chart;
		}
		var area = element("div", "sarg-columns");
		var max = Math.max.apply(null, data.map(function (item) { return item.bytes; }));
		data.forEach(function (item, index) {
			var value = item.bytes;
			var column = element("div", "sarg-column-item");
			column.title = item.label + ": " + formatBytes(value);
			column.appendChild(element("span", "sarg-column-value", formatBytes(value)));
			var bar = element("span", "sarg-column-bar");
			bar.style.height = Math.max(3, value / max * 112) + "px";
			bar.style.backgroundColor = palette[index % palette.length];
			column.appendChild(bar);
			column.appendChild(element("span", "sarg-column-label", item.label));
			area.appendChild(column);
		});
		chart.appendChild(area);
		return chart;
	}

	function render() {
		modernizeCalendarIcons();
		if (document.querySelector(".sarg-analytics")) return;
		var tables = document.querySelectorAll("table.sortable");
		var data = [];
		var table = null;
		for (var i = 0; i < tables.length && data.length < 2; i++) {
			data = extract(tables[i]);
			table = tables[i];
		}
		Array.prototype.forEach.call(document.querySelectorAll("table"), function (reportTable) {
			hideUnavailableE2guardianTimeColumns(reportTable);
			formatByteColumns(reportTable);
		});
		if (!table || data.length < 2 || !data.some(function (item) { return item.accesses > 0 || item.bytes > 0; })) return;
		var accessData = data.filter(function (item) { return item.accesses > 0; })
			.sort(function (left, right) { return right.accesses - left.accesses; }).slice(0, 7);
		var byteData = data.filter(function (item) { return item.bytes > 0; })
			.sort(function (left, right) { return right.bytes - left.bytes; }).slice(0, 7);

		var analytics = element("section", "sarg-analytics");
		analytics.setAttribute("aria-label", "Resumo visual do relatório");
		var heading = element("div", "sarg-analytics__heading");
		heading.appendChild(element("h2", "sarg-analytics__title", "Visão analítica"));
		heading.appendChild(element("span", "sarg-analytics__meta", "Principais registros deste relatório"));
		analytics.appendChild(heading);
		var grid = element("div", "sarg-analytics__grid");
		grid.appendChild(bars(accessData));
		grid.appendChild(pie(byteData));
		grid.appendChild(columns(byteData));
		analytics.appendChild(grid);
		var report = table.closest ? table.closest(".report") : null;
		if (!report || !report.parentNode) return;
		report.parentNode.insertBefore(analytics, report);
	}

	if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", render);
	else render();
}());
