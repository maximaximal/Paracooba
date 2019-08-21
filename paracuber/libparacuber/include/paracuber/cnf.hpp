#ifndef PARACUBER_CNF_HPP
#define PARACUBER_CNF_HPP

#include <string>
#include <string_view>

namespace paracuber {
class NetworkedNode;

/** @brief This class represents a CNF formula that can be transferred directly.
 *
 * A dimacs file can be sent, if it is the root formula. All appended cubes
 * are transferred only afterwards, the root formula is not touched again.
 */
class CNF
{
  public:
  /** @brief Construct a CNF from existing literals based on DIMACS file or on
   * previous CNF.
   */
  CNF(int64_t previous = -1, std::string_view dimacsFile = "");
  virtual ~CNF();

  int64_t getPrevious() { return m_previous; }
  std::string_view getDimacsFile() { return m_dimacsFile; }

  void send();

  private:
  int64_t m_previous = -1;
  std::string m_dimacsFile = "";
};
}

#endif
