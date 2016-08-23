#include <time.h>
#include <cmdline/cmdline.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <zlib.h>
#include <minizip/zip.h>

using namespace rapidjson;

int main(int argc, char *argv[])
{
	cmdline::parser cmd;
	cmd.set_program_name(PRODUCT_NAME);
	cmd.footer("extra files");
	cmd.add<std::string>("mark", 'm', "mark code", true);
	cmd.add<std::string>("input", 'i', "input file", true);
	cmd.add<std::string>("output", 'o', "output file", false);
	cmd.parse_check(argc, argv);
	std::vector<std::string> inputs = cmd.rest();
	std::string mark = cmd.get<std::string>("mark");
	std::string input = cmd.get<std::string>("input");
	std::string output;
	if (cmd.exist("output"))
	{
		output = cmd.get<std::string>("output");
	}
	else
	{
		size_t pos = input.find_last_of('.');
		if (pos == input.npos)
			pos = input.length();
		output = std::string(input.c_str(), 0, pos) + ".zip";
	}
	inputs.push_back(input);
	Document document;
	Document::AllocatorType& allocator = document.GetAllocator();
	document.SetObject();
	Value platform;
	platform.SetObject();
	platform.AddMember("name", Document::StringRefType(input.c_str()), allocator);
	platform.AddMember("sign", "", allocator);
	document.AddMember("win32", platform, allocator);
	document.AddMember("mark", Document::StringRefType(mark.c_str()), allocator);
	StringBuffer buffer;
	PrettyWriter<StringBuffer> writer(buffer);
	document.Accept(writer);

	zipFile zip = zipOpen(output.c_str(), APPEND_STATUS_CREATE);
	if (zip == NULL)
	{
		fprintf(stderr, "create file '%s' fail.\n", output.c_str());
		return -1;
	}
	zip_fileinfo fileinfo;
	memset(&fileinfo, 0, sizeof(fileinfo));
	time_t now = 0;
	tm *nowtime = localtime(&now);
	fileinfo.tmz_date.tm_year = nowtime->tm_year + 1900;
	fileinfo.tmz_date.tm_mon = nowtime->tm_mon;
	fileinfo.tmz_date.tm_mday = nowtime->tm_mday;
	fileinfo.tmz_date.tm_hour = nowtime->tm_hour;
	fileinfo.tmz_date.tm_min = nowtime->tm_min;
	fileinfo.tmz_date.tm_sec = nowtime->tm_sec;
	int result = ZIP_OK;
	do
	{
		result = zipOpenNewFileInZip(zip, "manifest.json", &fileinfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_BEST_COMPRESSION);
		if (result != ZIP_OK)
		{
			fprintf(stderr, "add file '%s' fail.\n", "manifest.json");
			break;
		}
		result = zipWriteInFileInZip(zip, buffer.GetString(), (unsigned int)buffer.GetSize());
		if (result != ZIP_OK)
		{
			fprintf(stderr, "write file '%s' fail.\n", "manifest.json");
			break;
		}
		result = zipCloseFileInZip(zip);
		if (result != ZIP_OK)
		{
			fprintf(stderr, "flush file '%s' fail.\n", "manifest.json");
			break;
		}
		char *filebytes = NULL;
		size_t byteslen = 0;
		for (size_t i = 0; i < inputs.size() ; ++i)
		{
			std::string &name = inputs[i];
			FILE *file = fopen(name.c_str(), "rb");
			if (file == NULL)
			{
				result = errno;
				fprintf(stderr, "read file '%s' fail.\n", name.c_str());
				break;
			}
			fseek(file, 0, SEEK_END);
			size_t len = ftell(file);
			fseek(file, 0, SEEK_SET);
			if (byteslen < len)
			{
				byteslen = byteslen < 16 ? 16 : byteslen;
				while (byteslen < len)
				{
					byteslen = byteslen * 2;
				}
				free(filebytes);
				filebytes = (char *)malloc(byteslen);
			}
			size_t nlen = fread(filebytes, 1, len, file);
			fclose(file);
			if (nlen != len)
			{
				result = -1;
				fprintf(stderr, "read file '%s' fail.\n", name.c_str());
				break;
			}
			result = zipOpenNewFileInZip(zip, name.c_str(), &fileinfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_BEST_COMPRESSION);
			if (result != ZIP_OK)
			{
				fprintf(stderr, "add file '%s' fail.\n", name.c_str());
				break;
			}
			result = zipWriteInFileInZip(zip, filebytes, len);
			if (result != ZIP_OK)
			{
				fprintf(stderr, "write file '%s' fail.\n", name.c_str());
				break;
			}
			result = zipCloseFileInZip(zip);
			if (result != ZIP_OK)
			{
				fprintf(stderr, "flush file '%s' fail.\n", name.c_str());
				break;
			}
		}
		free(filebytes);
	} while (false);
	bool del = result != ZIP_OK;
	result = zipClose(zip, NULL);
	del = del || result != ZIP_OK;
	if (del)
	{
		remove(output.c_str());
	}
	return result;
}