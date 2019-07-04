#ifndef PARACUBER_CNF_HPP
#define PARACUBER_CNF_HPP

#include <boost/archive/text_oarchive.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/version.hpp>

namespace paracuber {
/** @brief This class represents a CNF formula that can be transferred in binary
 * representation.
 *
 * The idea behind this class is to store the formula resulting from a parse
 * process. In CaDiCaL, the original literals can be found in the original field
 * in the external interface. This vector stays the same after parsing and can
 * be used to stream the literals to other nodes, while CaDiCaL can directly
 * process the formula locally. No copying required, beside the direct network
 * stream to a target.
 */
class CNF
{
  public:
  /** @brief Construct a CNF from existing literals based on DIMACS file or on
   * previous CNF.
   */
  explicit CNF(uint64_t previous = 0, std::string_view dimacsFile = "");
  virtual ~CNF();

  private:
  friend class boost::serialization::access;
  template<class Archive>
  void save(Archive& ar, const unsigned int version)
  {
    ar& m_previous;
    if(m_previous == 0) {
      // Open file for reading and stream into archive.
      using namespace boost::filesystem;
      path p{ m_dimacsFile };
      ifstream inStream{ p };
      ar& inStream;
    }
  }

  template<class Archive>
  void load(Archive& ar, const unsigned int version)
  {
    ar& m_previous;
    if(m_previous == 0) {
      // Open file for writing and stream from archive.
      using namespace boost::filesystem;
      path p = temp_directory_path() / unique_path();
      m_dimacsFile = p.string();
      ofstream outStream{ p };
      ar& outStream;
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()

  uint64_t m_previous;
  std::string m_dimacsFile;
};
}

BOOST_CLASS_VERSION(::paracuber::CNF, 0)

#endif
