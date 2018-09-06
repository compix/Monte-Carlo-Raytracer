#pragma once
#include <CLW.h>

class RTKernel
{
public:
	RTKernel() {}
	RTKernel(CLWKernel kernel)
		:m_kernel(kernel) {}

	void setArg(uint32_t idx, cl_mem val) { m_kernel.SetArg(idx, sizeof(cl_mem), &val); }
	void setArg(uint32_t idx, cl_int val)     { clSetKernelArg(m_kernel, idx, sizeof(cl_int), &val); }
	void setArg(uint32_t idx, cl_uint val)	  { clSetKernelArg(m_kernel, idx, sizeof(cl_uint), &val); }
	void setArg(uint32_t idx, cl_float val)   { clSetKernelArg(m_kernel, idx, sizeof(cl_float), &val); }
	void setArg(uint32_t idx, cl_float2 val)  { clSetKernelArg(m_kernel, idx, sizeof(cl_float2), &val); }
	void setArg(uint32_t idx, cl_float4 val)  { clSetKernelArg(m_kernel, idx, sizeof(cl_float4), &val); }
	void setArg(uint32_t idx, cl_double val)  { clSetKernelArg(m_kernel, idx, sizeof(cl_double), &val); }

	operator CLWKernel() const { return m_kernel; }
	operator cl_kernel() const { return m_kernel; }

protected:
	CLWKernel m_kernel;
};
