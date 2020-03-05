#include "utils/log.h"
#include <boost/core/null_deleter.hpp>
#include "boost/log/expressions.hpp"
#include "boost/log/support/date_time.hpp"

namespace bcm {

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

bool Log::init(LogConfig& conf, const char* fileHead)
{
    boost::shared_ptr<sinks::text_file_backend> logfile =
        boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = conf.directory + "/" + fileHead + "_%Y-%m-%d_%3N.log",
            keywords::rotation_size = 128*1024*1024,
            keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0)
        );

    logfile->set_file_collector(sinks::file::make_collector(
                    keywords::target = conf.directory,
                    keywords::max_files = 999,
                    keywords::max_size = 10ULL*1024*1024*1024,
                    keywords::min_free_space = 1024*1024*1024
                ));

    logfile->scan_for_files();
    logfile->auto_flush(conf.autoflush);

    typedef sinks::asynchronous_sink<
        sinks::text_file_backend,
        sinks::bounded_fifo_queue<1024*1024, sinks::block_on_overflow>
            > async_sink_t;

    typedef sinks::synchronous_sink<sinks::text_ostream_backend> sync_sink_t;

    boost::shared_ptr<async_sink_t> sink(new async_sink_t(logfile));

    logging::formatter myFormat = expr::stream
            << expr::format_date_time(timestamp, "%H:%M:%S.%f")
            << "|" << severity
            << "|" << threadId
            << "|" << expr::smessage;

    sink->set_formatter(myFormat);

    auto core = logging::core::get();
    core->add_sink(sink);

    logging::add_common_attributes();

    if (conf.level > static_cast<int>(LOGSEVERITY_FATAL)) {
        core->set_logging_enabled(false);
    } else if (conf.level < static_cast<int>(LOGSEVERITY_TRACE)) {
        core->set_filter(severity >= LOGSEVERITY_TRACE);
    } else {
        core->set_filter(severity >= static_cast<LogSeverity>(conf.level));
    }

    if (conf.console) {
        boost::shared_ptr<sync_sink_t> consoleSink(new sync_sink_t(boost::make_shared<sinks::text_ostream_backend>()));
        consoleSink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::cout,boost::null_deleter()));
        consoleSink->set_formatter(myFormat);
        core->add_sink(consoleSink);
    }

    return true;
}

void Log::flush()
{
    logging::core::get()->flush();
}

std::ostream& operator<< (std::ostream& strm, LogSeverity level)
{
    static const char* strings[] =
    {
        "trace",
        "debug",
        "info ",
        "warn ",
        "error",
        "fatal"
    };
    if (static_cast< std::size_t >(level) < sizeof(strings) / sizeof(*strings))
        strm << strings[level];
    else
        strm << static_cast< int >(level);
    return strm;
}

} // namespace bcm
