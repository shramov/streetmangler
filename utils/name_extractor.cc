/*
 * Copyright (C) 2011 Dmitry Marakasov
 *
 * This file is part of streetmangler.
 *
 * streetmangler is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * streetmangler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with streetmangler.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdexcept>
#include <sstream>

#include <expat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "name_extractor.hh"

NameExtractor::NameExtractor() {
}

NameExtractor::~NameExtractor() {
}

void NameExtractor::ParseFile(const char* filename) {
	int f;
	if ((f = open(filename, O_RDONLY)) == -1)
		throw std::runtime_error(std::string("Cannot open OSM file: ") + strerror(errno));

	try {
		Parse(f);
	} catch (...) {
		close(f);
		throw;
	}

	close(f);
}

void NameExtractor::ParseStdin() {
	return Parse(0);
}

void NameExtractor::Parse(int fd) {
	XML_Parser parser = NULL;

	if ((parser = XML_ParserCreate(NULL)) == NULL)
		throw std::runtime_error("cannot create XML parser");

	XML_SetElementHandler(parser, StartElement, EndElement);
	XML_SetUserData(parser, this);

	/* Parse file */
	try {
		char buf[65536];
		ssize_t len;
		do {
			if ((len = read(fd, buf, sizeof(buf))) < 0)
				throw std::runtime_error(std::string("read error: ") + strerror(errno));
			if (XML_Parse(parser, buf, len, len == 0) == XML_STATUS_ERROR)
				throw std::runtime_error(std::string("expat error: ") + XML_ErrorString(XML_GetErrorCode(parser)));
		} while (len != 0);
	} catch (std::runtime_error &e) {
		std::stringstream ss;
		ss << "error parsing input: " << e.what() << " at line " << XML_GetCurrentLineNumber(parser) << " pos " << XML_GetCurrentColumnNumber(parser);
		XML_ParserFree(parser);
		throw std::runtime_error(ss.str());
	} catch (...) {
		XML_ParserFree(parser);
		throw;
	}

	XML_ParserFree(parser);
}

void NameExtractor::StartElement(void* userData, const char* name, const char** atts) {
	NameExtractor* parser = static_cast<NameExtractor*>(userData);

	if (strcmp(name, "node") == 0 || strcmp(name, "way") == 0) {
		parser->addr_street_.clear();
		parser->name_.clear();
		parser->highway_.clear();
	}
	if (strcmp(name, "tag") != 0)
		return;

	std::string k, v;
	for (const char** att = atts; *att; att += 2) {
		if (att[0][0] == 'k')
			k = att[1];
		else if (att[0][0] == 'v')
			v = att[1];
	}

	if (k == "highway")
		parser->highway_ = v;
	else if (k == "name")
		parser->name_ = v;
	else if (k == "addr:street")
		parser->addr_street_ = v;
}

void NameExtractor::EndElement(void* userData, const char* name) {
	NameExtractor* parser = static_cast<NameExtractor*>(userData);

	enum { NODE, WAY, OTHER } tag = OTHER;

	if (strcmp(name, "node") == 0)
		tag = NODE;
	else if (strcmp(name, "way") == 0)
		tag = WAY;
	else
		return;

	if (!parser->addr_street_.empty())
		parser->ProcessName(parser->addr_street_);

	if (tag == WAY && !parser->highway_.empty() && !parser->name_.empty() &&
				parser->highway_ != "footway" &&
				parser->highway_ != "cycleway" &&
				parser->highway_ != "path" &&
				parser->highway_ != "track" &&
				parser->highway_ != "bus_stop" &&
				parser->highway_ != "emergency_access_point") {
		parser->ProcessName(parser->name_);
	}
}
