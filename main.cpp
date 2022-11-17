#include <QFile>
#include <QHash>
#include <QMap>
#include <QDebug>
#include <iostream>
#include <QtXlsx>

struct range
{
    range(uint32_t min, uint32_t max) : min(min), max(max){}
    bool contains(uint32_t value) const {
        return value >= min && value <= max;
    }

    bool operator < (const range &rhs) const {
        return min < rhs.min;
    }

    bool operator == (const range &rhs) const {
        return min == rhs.min && max == rhs.max;
    }

    uint32_t min;
    uint32_t max;
};

enum class ARFCN {
    INVALID = -1,
    GSM450,
    GSM480,
    GSM750,
    GSM850,
    PGSM,
    EGSM,
    GSMR,
    DCS1800,
};

static QHash<range, ARFCN> getRangeHash()
{
    static QHash<range, ARFCN> ranges {{{259, 293}, ARFCN::GSM450},
                                       {{306, 340}, ARFCN::GSM480},
                                       {{438, 511}, ARFCN::GSM750},
                                       {{128, 251}, ARFCN::GSM850},
                                       {{1, 124}, ARFCN::PGSM},
                                       {{306, 340}, ARFCN::EGSM},
                                       {{940, 974}, ARFCN::GSMR},
                                       {{512, 885}, ARFCN::DCS1800}
                                      };
    return ranges;
}

static QHash<ARFCN, float (*)(float)> getUplinkFunctionHash()
{
    static QHash<ARFCN, float (*)(float)> hash {
        {ARFCN::GSM450, [](float value)->float {
          return  450.6f + 0.2f * (value - 259.f);
        }},
        {ARFCN::GSM480, [](float value)->float {
          return  479.f + 0.2f * (value - 306.f);
        }},
        {ARFCN::GSM750, [](float value)->float {
          return  747.2f + 0.2f * (value - 438.f);
        }},
        {ARFCN::GSM850, [](float value)->float {
          return  824.2f + 0.2f * (value - 128.f);
        }},
        {ARFCN::PGSM, [](float value)->float {
          return  890.f + 0.2f * value;
        }},
        {ARFCN::EGSM, [](float value)->float {
          return  890.f + 0.2f * (value - 1024.f);
        }},
        {ARFCN::GSMR, [](float value)->float {
          return  890.f + 0.2f * (value - 1024.f);
        }},
        {ARFCN::DCS1800, [](float value)->float {
          return  1710.2f + 0.2f * (value - 512.f);
        }}
    };
    return hash;
}

static QHash<ARFCN, float(*)(float)> getDownlinkFunctionHash()
{
    QHash<ARFCN, float(*)(float)> hash {
        {ARFCN::GSM450, [](float value)->float {
          return value + 10.f;
        }},
        {ARFCN::GSM480, [](float value)->float {
          return value + 10.f;
        }},
        {ARFCN::GSM750, [](float value)->float {
          return value + 30.f;
        }},
        {ARFCN::GSM850, [](float value)->float {
          return value + 45.f;
        }},
        {ARFCN::PGSM, [](float value)->float {
          return value + 45.f;
        }},
        {ARFCN::EGSM, [](float value)->float {
          return value + 45.f;
        }},
        {ARFCN::GSMR, [](float value)->float {
          return value + 45.f;
        }},
        {ARFCN::DCS1800, [](float value)->float {
          return value + 95.f;
        }}
    };
    return hash;
}

inline uint qHash(ARFCN v) {
    return qHash(static_cast<int>(v));
}

inline uint qHash(const range &r) {
    return qHash(r.min) + qHash(r.max);
}

inline QDebug operator << (QDebug debug, ARFCN val) {
    debug << static_cast<int>(val);
    return debug;
}


QString getUplinkDownlink(float value)
{
    auto ranges = getRangeHash();

    ARFCN standart = {ARFCN::INVALID};
    for (auto it = ranges.begin(); it != ranges.end(); ++it) {
        if (it.key().contains(value)) {
            standart = it.value();
            break;
        }
    }

    auto uplinkFn = getUplinkFunctionHash().value(standart);
    auto downlinkFn = getDownlinkFunctionHash().value(standart);

    float uplink = uplinkFn(value);
    float downlink = downlinkFn(uplink);

    return QString::number(downlink) + "/" + QString::number(uplink);
}

QStringList readFileContent(const QString &path)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    QStringList r = QString(file.readAll()).remove('\r').split('\n', Qt::SkipEmptyParts);
    file.close();
    return r;
}

void writeExcelFile(const QList<QStringList> &data, const QString &filename)
{
    QXlsx::Document document;

    QXlsx::Format fmt;
    fmt.setTextWarp(true);

    int forwardCol = 15;

    for (int row = 0; row < data.size(); ++row) {
        for (int col = 0; col < data[row].size(); ++col) {
            data[row][col].contains(10) ? document.write(row + 1, forwardCol++, data[row][col], fmt) :
                                          document.write(row + 1, col + 1, data[row][col]);
        }
        forwardCol = 15;
    }

    document.saveAs(filename);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        qDebug() << "Program requires minimum 1 argument at least...";
        return -1;
    }
    auto rows = readFileContent(argv[1]);

    QList<QStringList> result;
    for (const QString &row : rows) {
        QStringList freq = row.split(' ', Qt::SkipEmptyParts);
        QStringList tStr;

        for (const QString &f : qAsConst(freq)) {
            tStr << f;
        }
        tStr << freq.join(10);

        QStringList upDownLink;
        for (const QString &f : freq) {
            float frequency = f.toFloat();
            upDownLink.push_back(getUplinkDownlink(frequency));
        }
        tStr << upDownLink.join(10);
        result << tStr;
    }
    writeExcelFile(result, argc == 3 ? QString(argv[2]) + ".xlsx" : "result.xlsx");
    return 0;
}
