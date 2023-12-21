#ifndef __SIMPLETASK_H__
#define __SIMPLETASK_H__

class SimpleTask
{
    public:
        SimpleTask(int hz): _hz(hz), _interval((1000) / hz) {};
        ~SimpleTask() {};

        void loop();
        virtual void run(int count) = 0;

        int next_run_ms() { return this->_last + this->_interval; };

    protected:
        int _hz;
        int _interval;

    private:
        int _count = 0;
        int _last = 0;
};

#endif // __SIMPLETASK_H__