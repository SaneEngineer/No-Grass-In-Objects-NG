#pragma once

class InvalidOperationException : public std::exception
{
private:
    std::string msg;

public:
    InvalidOperationException(const std::string& message = "") : msg(message)
    {
    }

    const char * what() const noexcept
    {
        return msg.c_str();
    }
};

class NullReferenceException : public std::exception
{
private:
    std::string msg;

public:
    NullReferenceException(const std::string& message = "") : msg(message)
    {
    }

    const char * what() const noexcept
    {
        return msg.c_str();
    }
};

class InvalidCastException : public std::exception
{
private:
	std::string msg;

public:
	InvalidCastException(const std::string& message = "") :
		msg(message)
	{
	}

	const char* what() const noexcept
	{
		return msg.c_str();
	}
};

class ArgumentOutOfRangeException : public std::exception
{
private:
	std::string msg;

public:
	ArgumentOutOfRangeException(const std::string& message = "") :
		msg(message)
	{
	}

	const char* what() const noexcept
	{
		return msg.c_str();
	}
};

class OutOfMemoryException : public std::exception
{
private:
	std::string msg;

public:
	OutOfMemoryException(const std::string& message = "") :
		msg(message)
	{
	}

	const char* what() const noexcept
	{
		return msg.c_str();
	}
};


class AmbiguousMatchException : public std::exception
{
private:
	std::string msg;

public:
	AmbiguousMatchException(const std::string& message = "") :
		msg(message)
	{
	}

	const char* what() const noexcept
	{
		return msg.c_str();
	}
};
