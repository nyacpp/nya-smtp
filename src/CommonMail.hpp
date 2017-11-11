#ifndef COMMONMAIL_HPP
#define COMMONMAIL_HPP

#include <QByteArray>
#include <memory>
#include "nya.hpp"

namespace Nya
{
inline bool IsSpecialChar(char x) { return x < 32 || x > 126 || x == '=' || x == '?'; }
char GuessEncoding(const QString& s);
QByteArray CreateEntity(const QByteArray& key, const QString& value, const QByteArray& prefix = QByteArray());
bool IsText(const QByteArray& contentType);
}
#endif // COMMONMAIL_HPP
