/*
 *  DESPOT_Functors.h
 *
 *  Created by Tobias Wood on 16/08/2012.
 *  Copyright (c) 2012-2013 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef DESPOT_Functors_h
#define DESPOT_Functors_h

#include <vector>
#include <array>
#include <map>
#include <iostream>
#include <exception>
#include <string>
#include <memory>
#include <Eigen/Dense>

#include "DESPOT.h"

using namespace std;
using namespace Eigen;

//******************************************************************************
#pragma mark Signal Functors
//******************************************************************************
enum class Components {
	One, Two, Three
};

class SignalFunctor {
	public:
		Components m_nC;
		double m_TR;
		ArrayXd m_flip;
		SignalFunctor(const ArrayXd &flip, const double TR, const Components nC);
		virtual ArrayXd signal(const VectorXd &p, const double B1 = 1., const double f0 = 0.) const = 0;
		virtual size_t size() const { return m_flip.rows(); }
};

class SPGR_Functor : public SignalFunctor {
	public:
		SPGR_Functor(const ArrayXd &flip, const double TR, const Components nC);
		ArrayXd signal(const VectorXd &p, const double B1 = 1., const double f0 = 0.) const override;
};
class SPGR_Finite_Functor : public SignalFunctor {
	public:
		double m_Trf, m_TE;
		SPGR_Finite_Functor(const ArrayXd &flip, const double TR, const double Trf, const double TE, const Components nc);
		ArrayXd signal(const VectorXd &p, const double B1 = 1., const double f0 = 0.) const override;
};
class SSFP_Functor : public SignalFunctor {
	public:
		ArrayXd m_phases;
		SSFP_Functor(const ArrayXd &flip, const double TR, const ArrayXd &phases, const Components nC);
		ArrayXd signal(const VectorXd &p, const double B1 = 1., const double f0 = 0.) const override;
		size_t size() const override;
};
class SSFP_Finite_Functor : public SSFP_Functor {
	public:
		double m_Trf, m_TE;
		SSFP_Finite_Functor(const ArrayXd &flip, const double TR, const double Trf, const ArrayXd &phases, const Components nC);
		ArrayXd signal(const VectorXd &p, const double B1 = 1., const double f0 = 0.) const override;
};
		
shared_ptr<SignalFunctor> parseSPGR(const Nifti &img, const bool prompt, const Components nC, const bool finite);
shared_ptr<SignalFunctor> parseSSFP(const Nifti &img, const bool prompt, const Components nC, const bool finite);

//******************************************************************************
#pragma mark Optimisation Functor Base Class
//******************************************************************************
// From Nonlinear Tests in Eigen 
template<typename _Scalar, int NX=Dynamic, int NY=Dynamic>
class Functor {
	public:
		typedef _Scalar Scalar;
		enum {
			InputsAtCompileTime = NX,
			ValuesAtCompileTime = NY
		};
		typedef Matrix<Scalar,InputsAtCompileTime,1> InputType;
		typedef Matrix<Scalar,ValuesAtCompileTime,1> ValueType;
		typedef Matrix<Scalar,ValuesAtCompileTime,InputsAtCompileTime> JacobianType;
		
		const long m_inputs, m_values;
		
		Functor() : m_inputs(InputsAtCompileTime), m_values(ValuesAtCompileTime) {}
		Functor(long inputs, long values) : m_inputs(inputs), m_values(values) {}
		Functor(long values) : m_inputs(InputsAtCompileTime), m_values(values) {}
		
		virtual ~Functor() {};
		
		virtual const long inputs() const { return m_inputs; }
		virtual const long values() const { return m_values; }
		
		virtual int operator()(const Ref<VectorXd> &params, Ref<ArrayXd> diffs) = 0;
		
		virtual const ArrayXd theory(const Ref<VectorXd> &params) = 0;
		virtual const ArrayXd actual() const = 0;
};

//******************************************************************************
#pragma mark Basic DESPOT Functor
//******************************************************************************
class DESPOTFunctor : public Functor<double> {
	public:
		enum class FieldStrength {
			Three, Seven, Unknown
		};
		static const string to_string(const FieldStrength& f) {
			switch (f) {
				case FieldStrength::Three: return "3";
				case FieldStrength::Seven: return "7";
				case FieldStrength::Unknown: return "User";
			}
		}
		enum class Scaling {
			Global, NormToMean
		};
		static const string to_string(const Scaling &p) {
			switch (p) {
				case Scaling::Global: return "global";
				case Scaling::NormToMean: return "normalised to per signal mean";
			}
		}
		enum class OffResMode {
			Map = 0, Single, SingleSymmetric
		};
	
	protected:
		const FieldStrength m_fieldStrength;
		const OffResMode m_offRes;
		const Scaling m_scaling;
		size_t m_nV;
		vector<shared_ptr<SignalFunctor>> m_signals;
		vector<ArrayXd> m_actual, m_theory;
		vector<string> m_names; // Subclasses responsible for initialising this
		const bool m_debug;
	
		ArrayXXd offResBounds() {
			ArrayXXd b(nOffRes(), 2);
			for (size_t i = 0; i < nOffRes(); i++) {
				if (m_offRes == OffResMode::SingleSymmetric)
					b(i, 0) = 0;
				else
					b(i, 0) = -0.5 / m_signals.at(i)->m_TR;
				b(i, 1) = 0.5 / m_signals.at(i)->m_TR;
			}
			return b;
		}
	
		ArrayXXd PDBounds() {
			ArrayXXd b(nPD(), 2);
			for (size_t i = 0; i < nPD(); i++) {
				b(i, 0) = 1.e4;
				b(i, 1) = 5.e6;
			}
			return b;
		}
		
		virtual const ArrayXXd defaultBounds() = 0;
		
	public:
		double m_f0, m_B1;
		const size_t nOffRes() const {
			switch (m_offRes) {
				case OffResMode::Map: return 0;
				case OffResMode::Single: return 1;
				case OffResMode::SingleSymmetric: return 1;
			}
		}
		
		const size_t nPD() const {
			switch (m_scaling) {
				case Scaling::Global:     return 1;
				case Scaling::NormToMean: return 0;
			}
		}
		
		virtual const size_t nP() const = 0;
		virtual const bool constraint(const VectorXd &params) const = 0;
		const long inputs() const override { return this->nP() + nOffRes() + nPD(); }
		const long values() const override { return m_nV; }
		
		DESPOTFunctor(vector<shared_ptr<SignalFunctor>> &signals_in,
					  const FieldStrength &tesla,
					  const OffResMode &offRes = OffResMode::Single,
					  const Scaling &s = Scaling::NormToMean,
		              const bool &debug = false) :
			m_signals(signals_in),
			m_fieldStrength(tesla), m_offRes(offRes),
			m_scaling(s), m_debug(debug)
		{
			m_actual.reserve(m_signals.size());
			m_theory.reserve(m_signals.size());
			m_nV = 0;
			for (size_t i = 0; i < m_signals.size(); i++) {
				m_actual.emplace_back(m_signals.at(i)->size());
				m_theory.emplace_back(m_signals.at(i)->size());
				m_nV += m_signals.at(i)->size();
			}
		}
		
		const vector<string> &names() { return m_names; }
		shared_ptr<SignalFunctor> &signal(const size_t i) { return m_signals.at(i); }
		ArrayXd &actual(const size_t s) { return m_actual.at(s); }
		const ArrayXd actual() const {
			ArrayXd v(values());
			int index = 0;
			if (m_debug) cout << __PRETTY_FUNCTION__ << endl;
			for (size_t i = 0; i < m_actual.size(); i++) {
				v.segment(index, m_actual.at(i).size()) = m_actual.at(i);
				index += m_actual.at(i).size();
				if (m_debug) cout << v.transpose() << endl;
			}
			return v;
		}
		
		int operator()(const Ref<VectorXd> &params, Ref<ArrayXd> diffs) override {
			eigen_assert(diffs.size() == values());
			ArrayXd t = theory(params);
			ArrayXd s = actual();
			diffs = t - s;
			if (m_debug) {
				cout << endl << __PRETTY_FUNCTION__ << endl;
				cout << "p:      " << params.transpose() << endl;
				cout << "t:      " << t.transpose() << endl;
				cout << "s:      " << s.transpose() << endl;
				cout << "Diffs:  " << diffs.transpose() << endl;
				cout << "Sum:    " << diffs.square().sum() << endl;
			}
			return 0;
		}
};

//******************************************************************************
#pragma mark mcDESPOT Functor
//******************************************************************************
class mcDESPOT : public DESPOTFunctor {
	public:
		static const string to_string(const Components& c) {
			switch (c) {
				case Components::One: return "1";
				case Components::Two: return "2";
				case Components::Three: return "3";
			}
		};
	protected:
		const Components m_components;
	
	public:
		const size_t nP() const override {
			switch (m_components) {
				case Components::One: return 2;
				case Components::Two: return 6;
				case Components::Three: return 9;
			}
		}
		
		const ArrayXXd defaultBounds() override {
			ArrayXXd b(inputs(), 2);
			switch (m_fieldStrength) {
				case FieldStrength::Three:
					switch (m_components) {
						case Components::One:   b.block(0, 0, 2, 2) << 0.1, 4.0, 0.01, 1.5; break;
						case Components::Two:   b.block(0, 0, 6, 2) << 0.1, 0.25, 0.002, 0.03, 0.7, 4.0, 0.075, 0.145, 0.05, 0.3, 0.0, 0.95; break;
						case Components::Three: b.block(0, 0, 9, 2) << 0.1, 0.25, 0.002, 0.03, 0.7, 2.0, 0.075, 0.145, 3.5, 4.0, 0.8, 1.5, 0.05, 0.3, 0.001, 0.3, 0.0, 0.95; break;
					} break;
				case FieldStrength::Seven:
					switch (m_components) {
						case Components::One:   b.block(0, 0, 2, 2) << 0.1, 4.0, 0.01, 2.0; break;
						case Components::Two:   b.block(0, 0, 6, 2) << 0.1, 0.5, 0.001, 0.025, 1.0, 4.0, 0.04, 0.08, 0.01, 0.25, 0.001, 1.0; break;
						case Components::Three: b.block(0, 0, 9, 2) << 0.1, 0.5, 0.001, 0.025, 1.0, 2.5, 0.04, 0.08, 3., 4.5, 0.5, 2.0, 0.01, 0.25, 0.001, 0.4, 0.001, 1.0; break;
					} break;
				case FieldStrength::Unknown:
					switch (m_components) {
						case Components::One:   b.block(0, 0, 2, 2).setZero(); break;
						case Components::Two:   b.block(0, 0, 6, 2).setZero(); break;
						case Components::Three: b.block(0, 0, 9, 2).setZero(); break;
					} break;
			}
			b.block(nP(), 0, nOffRes(), 2) = offResBounds();
			b.block(nP() + nOffRes(), 0, nPD(), 2) = PDBounds();
			return b;
		}
		
		const ArrayXd defaultThresholds() {
			ArrayXd m(inputs());
			switch (m_components) {
				case Components::One: m.head(nP()) << 0.05, 0.05; break;
				case Components::Two: m.head(nP()) << 0.5, 0.5, 0.5, 0.5, 0.5, 0.05; break;
				case Components::Three: m.head(nP()) << 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.05, 0.05; break;
			}
			m.segment(nP(), nOffRes()).setConstant(0.1);
			m.tail(nPD()).setConstant(0.1);
			return m;
		}
		
		mcDESPOT(const Components &c, vector<shared_ptr<SignalFunctor>> &data,
				 const FieldStrength &tesla, const OffResMode &offRes, const Scaling &s = Scaling::NormToMean,
				 const bool &debug = false) :
			DESPOTFunctor(data, tesla, offRes, s, debug),
			m_components(c)
		{
			m_names.reserve(inputs());
			switch (c) {
				case Components::One: m_names = {"T1", "T2"}; break;
				case Components::Two: m_names = {"T1_a", "T2_a", "T1_b", "T2_b", "tau_a", "f_a"}; break;
				case Components::Three: m_names = {"T1_a", "T2_a", "T1_b", "T2_b", "T1_c", "T2_c", "tau_a", "f_a", "f_c"}; break;
			}
			for (size_t i = 0; i < nOffRes(); i++)
				m_names.emplace_back("f0_" + std::to_string(i));
			for (size_t i = 0; i < nPD(); i++)
				m_names.emplace_back("PD_" + std::to_string(i));
		}
		
		const bool constraint(const VectorXd &params) const override {
			// Negative PD or T1/T2 makes no sense
			if ((params[0] <= 0.) || (params[1] <= 0.))
				return false;
			
			if (m_components == Components::One) {
				return true;
			} else if (m_components == Components::Two) {
				// Check that T1_a, T2_a < T1_b, T2_b and that f_a makes sense
				if ((params[0] < params[2]) &&
					(params[1] < params[3]) &&
					(params[5] <= 1.0))
					return true;
				else
					return false;
			} else if (m_components == Components::Three) {
				// Check that T1/2_a < T1/2_b < T1/2_c and that f_a + f_c makes sense 
				if ((params[0] < params[2]) &&
					(params[1] < params[3]) &&
					(params[2] < params[4]) &&
					(params[3] < params[5]) &&
					((params[7] + params[8]) <= 1.0))
					return true;
				else
					return false;
			} else
				return true;
		}
		
		const ArrayXd theory(const Ref<VectorXd> &params) override {
			ArrayXd t(values());
			if (m_debug) cout << endl << __PRETTY_FUNCTION__ << endl << "Params: " << params.transpose() << endl;
			for (size_t i = 0; i < m_signals.size(); i++) {
				double f0;
				if (m_offRes == OffResMode::Map)
					f0 = m_f0;
				else
					f0 = params[nP()];
				ArrayXd sig = m_signals.at(i)->signal(params.head(nP()), m_B1, f0);
				switch (m_scaling) {
					case (Scaling::NormToMean) : m_theory.at(i) = sig / sig.mean(); break;
					case (Scaling::Global)     : m_theory.at(i) = sig * params(nP() + nOffRes()); break;
				}
			}

			int index = 0;
			for (size_t i = 0; i < m_signals.size(); i++) {
				t.segment(index, m_actual.at(i).size()) = m_theory.at(i);
				if (m_debug) cout << m_theory.at(i).transpose() << endl;
				index += m_theory.at(i).size();
			}
			
			return t;
		}
};

class mcFinite : public mcDESPOT {
	public:
		const size_t nP() const override {
			return mcDESPOT::nP() + 1;
		}
				
		const ArrayXXd defaultBounds() override {
			ArrayXXd b(inputs(), 2);
			switch (m_fieldStrength) {
				case FieldStrength::Three:
					switch (m_components) {
						case Components::One:   b.block(0, 0, 3, 2) << 0.1, 4.0, 0.01, 1.5, 0., 100.; break;
						case Components::Two:   b.block(0, 0, 7, 2) << 0.1, 0.25, 0.002, 0.03, 0.7, 4.0, 0.075, 0.145, 0.05, 0.3, 0.0, 0.95, 0., 100.; break;
						case Components::Three: b.block(0, 0, 10, 2) << 0.1, 0.25, 0.002, 0.03, 0.7, 2.0, 0.075, 0.145, 3.5, 4.0, 0.8, 1.5, 0.05, 0.3, 0.001, 0.3, 0.001, 0.95, 0., 100.; break;
					} break;
				case FieldStrength::Seven:
					switch (m_components) {
						case Components::One:   b.block(0, 0, 3, 2) << 0.1, 4.0, 0.01, 2.0, 0., 100.; break;
						case Components::Two:   b.block(0, 0, 7, 2) << 0.1, 0.5, 0.001, 0.025, 1.0, 2.5, 0.04, 0.08, 0.01, 0.25, 0.001, 1.0, 0., 100.; break;
						case Components::Three: b.block(0, 0, 10, 2) << 0.1, 0.5, 0.001, 0.025, 1.0, 2.5, 0.04, 0.08, 3., 4.5, 0.5, 2.0, 0.01, 0.25, 0.001, 0.4, 0.001, 1.0, 0., 100.; break;
					} break;
				case FieldStrength::Unknown:
					switch (m_components) {
						case Components::One:   b.block(0, 0, 3, 2).setZero(); break;
						case Components::Two:   b.block(0, 0, 7, 2).setZero(); break;
						case Components::Three: b.block(0, 0, 10, 2).setZero(); break;
					} break;
			}
			b.block(nP(), 0, nOffRes(), 2) = offResBounds();
			b.block(nP() + nOffRes(), 0, nPD(), 2) = PDBounds();
			return b;
		}
		
		const ArrayXd defaultThresholds() {
			ArrayXd m(inputs());
			switch (m_components) {
				case Components::One: m.head(nP()) << 0.05, 0.05, 1.0; break;
				case Components::Two: m.head(nP()) << 0.5, 0.5, 0.5, 0.5, 0.5, 0.05, 1.0; break;
				case Components::Three: m.head(nP()) << 0.5, 0.5, 0.5, 0.5, 0.75, 0.75, 0.5, 0.05, 0.05, 1.0; break;
			}
			m.segment(nP(), nOffRes()).setConstant(0.1);
			m.tail(nPD()).setConstant(0.1);
			return m;
		}
		
		mcFinite(const Components &c, vector<shared_ptr<SignalFunctor>> &data,
				 const FieldStrength &tesla, const OffResMode &offRes, const Scaling &s = Scaling::NormToMean,
				 const bool &debug = false) :
			mcDESPOT(c, data, tesla, offRes, s, debug)
		{
			m_names.insert(m_names.begin() + nP() - 1, "delta_f");
		}
};
//******************************************************************************
#pragma mark DESPOT2FM Functor
//******************************************************************************
class DESPOT2FM : public DESPOTFunctor {
	protected:
		bool m_finite;
		
	public:
		double m_T1;
		const size_t nP() const override {
			return 1;
		}
		
		const ArrayXXd defaultBounds() override {
			ArrayXXd b(inputs(), 2);
			switch (m_fieldStrength) {
				case FieldStrength::Three: b.block(0, 0, 1, 2) << 0.010, 1.5; break;
				case FieldStrength::Seven: b.block(0, 0, 1, 2) << 0.005, 2.0; break;
				case FieldStrength::Unknown: b.block(0, 0, 1, 2).setZero(); break;
			}
			b.block(nP(), 0, nOffRes(), 2) = offResBounds();
			b.block(nP() + nOffRes(), 0, nPD(), 2) = PDBounds();
			return b;
		}
		
		const ArrayXd defaultThresholds() {
			ArrayXd m(inputs());
			m.head(nP()) << 0.05;
			m.segment(nP(), nOffRes()).setConstant(0.1);
			m.tail(nPD()).setConstant(0.1);
			return m;
		}
		
		const bool constraint(const VectorXd &params) const override {
			if (params[0] < 0.)
				return false;
			else
				return true;
		}
		
		DESPOT2FM(vector<shared_ptr<SignalFunctor>> &data, const double T1,
				  const FieldStrength& tesla, const OffResMode &offRes, const Scaling &s = Scaling::NormToMean,
				  const bool &finite = false, const bool &debug = false) :
			DESPOTFunctor(data, tesla, offRes, s, debug), m_T1(T1), m_finite(finite)
		{
			m_names.resize(inputs());
			m_names.at(0) = "T2";
			for (size_t i = 0; i < nOffRes(); i++)
				m_names.at(1 + i) = "f0_" + std::to_string(i);
			for (size_t i = 0; i < nPD(); i++)
				m_names.at(1 + nOffRes() + i) = "PD_" + std::to_string(i);
		}
		
		const ArrayXd theory(const Ref<VectorXd> &params) override {
			VectorXd T1T2(2);
			T1T2 << m_T1, params[0];
			if (m_debug) cout << endl << __PRETTY_FUNCTION__ << endl;
			for (size_t i = 0; i < m_signals.size(); i++) {
				double f0;
				if (m_offRes == OffResMode::Map)
					f0 = m_f0;
				else
					f0 = params[nP()];
				ArrayXd sig = m_signals.at(i)->signal(T1T2, m_B1, f0);
				switch (m_scaling) {
					case (Scaling::NormToMean) : m_theory.at(i) = sig / sig.mean(); break;
					case (Scaling::Global)     : m_theory.at(i) = sig * params(nP() + nOffRes()); break;
				}
				if (m_debug) cout << "m_theory.at(" << i << "): " << m_theory.at(i).transpose() << endl;
			}
			int index = 0;
			ArrayXd t(values()); t.setZero();
			for (size_t i = 0; i < m_signals.size(); i++) {
				t.segment(index, m_actual.at(i).size()) = m_theory.at(i);
				index += m_theory.at(i).size();
				if (m_debug) {
					cout << "m_theory.at(" << i << "): " << m_theory.at(i).transpose() << endl;
					cout << "t: " << t.transpose() << endl;
				}
			}
			
			return t;
		}
};

#endif
